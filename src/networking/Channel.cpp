#include "Channel.hpp"
#include "Client.hpp"
#include "NetworkHost.hpp"

#include <cctype>

std::string Channel::ircLower(const std::string& value)
{
	std::string out = value;
	for (std::string::size_type i = 0; i < out.size(); ++i) {
		char c = out[i];
		if (c >= 'A' && c <= 'Z') {
			out[i] = static_cast<char>(c - 'A' + 'a');
		} else if (c == '[') {
			out[i] = '{';
		} else if (c == ']') {
			out[i] = '}';
		} else if (c == '\\') {
			out[i] = '|';
		} else if (c == '~') {
			out[i] = '`';
		}
	}
	return out;
}

bool Channel::isValidName(const std::string& channelName)
{
	if (channelName.size() < 2 || channelName.size() > 50) {
		return false;
	}
	char prefix = channelName[0];
	if (prefix != '#' && prefix != '&' && prefix != '+' && prefix != '!') {
		return false;
	}
	for (std::string::size_type i = 1; i < channelName.size(); ++i) {
		char c = channelName[i];
		if (c == ' ' || c == '\a' || c == ',') {
			return false;
		}
	}
	return true;
}

Channel::Type Channel::prefixType(const std::string& channelName)
{
	if (channelName.empty()) {
		return TINVAL;
	}
	char c = channelName[0];
	if (c == '#') return REGULAR;
	if (c == '&') return LOCAL;
	if (c == '+') return NOMODE;
	if (c == '!') return IOWN;
	return TINVAL;
}

void Channel::initFromName(const std::string& channelName)
{
	m_name = channelName;
	m_type = prefixType(channelName);
	m_topic.clear();
	m_modes = 0;
	m_key.clear();
	m_userLimit = 0;
	m_markedForDeath = false;
	m_users.clear();
	m_operators.clear();
	m_invites.clear();
}

Channel::Channel(const std::string& channelName)
	: m_host(&NetworkHost::GetGlobal())
{
	initFromName(channelName);
}

Channel::Channel(const std::string& channelName, NetworkHost& host)
	: m_host(&host)
{
	initFromName(channelName);
}

Channel::~Channel()
{
}

std::map<int, Client*>& Channel::getUsers()
{
	return m_users;
}

const std::map<int, Client*>& Channel::getUsers() const
{
	return m_users;
}

Client* Channel::getUserByNick(const std::string& nick) const
{
	const std::string want = ircLower(nick);
	for (std::map<int, Client*>::const_iterator it = m_users.begin();
		 it != m_users.end(); ++it) {
		Client* c = it->second;
		if (c == 0 || c->isMarkedForDeath()) {
			continue;
		}
		if (ircLower(c->nick) == want) {
			return const_cast<Client*>(c);
		}
	}
	return 0;
}

bool Channel::hasUser(int fd) const
{
	std::map<int, Client*>::const_iterator it = m_users.find(fd);
	if (it == m_users.end() || it->second == 0) {
		return false;
	}
	if (it->second->isMarkedForDeath()) {
		return false;
	}
	return true;
}

void Channel::addUser(Client* client)
{
	if (client == 0) {
		return;
	}
	int fd = client->getSocket();
	if (m_users.find(fd) != m_users.end()) {
		return;
	}
	m_users[fd] = client;
}

void Channel::removeMember(int fd, const std::string& notifyLine)
{
	std::map<int, Client*>::iterator it = m_users.find(fd);
	if (it == m_users.end()) {
		return;
	}
	m_users.erase(it);
	m_operators.erase(fd);
	if (!notifyLine.empty()) {
		broadcast(notifyLine);
	}
}

void Channel::rmUser(Client* client, const std::string& notifyLine)
{
	if (client == 0) {
		return;
	}
	removeMember(client->getSocket(), notifyLine);
}

void Channel::rmUser(int fd, const std::string& notifyLine)
{
	removeMember(fd, notifyLine);
}

void Channel::broadcast(const std::string& message, Client* except)
{
	if (m_host == 0 || message.empty()) {
		return;
	}
	for (std::map<int, Client*>::iterator it = m_users.begin();
		 it != m_users.end(); ++it) {
		Client* c = it->second;
		if (c == 0 || c->isMarkedForDeath()) {
			continue;
		}
		if (except != 0 && c == except) {
			continue;
		}
		m_host->enqueueSend(c, message);
	}
}

const std::string& Channel::getName() const
{
	return m_name;
}

Channel::Type Channel::getPrefix() const
{
	return m_type;
}

const std::string& Channel::getTopic() const
{
	return m_topic;
}

void Channel::setTopic(const std::string& topic)
{
	m_topic = topic;
}

void Channel::setMode(Mode mode, bool enabled)
{
	int bit = 0;
	switch (mode) {
	case INVITEONLY: bit = 1; break;
	case PROTECTEDTOPIC: bit = 2; break;
	case KEY: bit = 4; break;
	case USERLIMIT: bit = 8; break;
	default: return;
	}
	if (enabled) {
		m_modes |= bit;
	} else {
		m_modes &= ~bit;
	}
}

bool Channel::isModeSet(Mode mode) const
{
	int bit = 0;
	switch (mode) {
	case INVITEONLY: bit = 1; break;
	case PROTECTEDTOPIC: bit = 2; break;
	case KEY: bit = 4; break;
	case USERLIMIT: bit = 8; break;
	default: return false;
	}
	return (m_modes & bit) != 0;
}

bool Channel::isOperator(Client* client) const
{
	if (client == 0) {
		return false;
	}
	return m_operators.find(client->getSocket()) != m_operators.end();
}

void Channel::addOperator(Client* client)
{
	if (client == 0 || isOperator(client)) {
		return;
	}
	m_operators.insert(client->getSocket());
}

void Channel::rmOperator(Client* client)
{
	if (client == 0) {
		return;
	}
	m_operators.erase(client->getSocket());
}

void Channel::invite(Client* client)
{
	if (client == 0 || isInvited(client)) {
		return;
	}
	m_invites.insert(client->getSocket());
}

bool Channel::isInvited(Client* client) const
{
	if (client == 0) {
		return false;
	}
	return m_invites.find(client->getSocket()) != m_invites.end();
}

void Channel::setKey(const std::string& key)
{
	m_key = key;
}

const std::string& Channel::getKey() const
{
	return m_key;
}

void Channel::clearKey()
{
	m_key.clear();
}

int Channel::getUserLimit() const
{
	return m_userLimit;
}

void Channel::setUserLimit(int limit)
{
	m_userLimit = limit;
}

void Channel::clearUserLimit()
{
	m_userLimit = 0;
}

void Channel::markForDeath()
{
	m_markedForDeath = true;
}

bool Channel::isMarkedForDeath() const
{
	return m_markedForDeath;
}

std::ostream& operator<<(std::ostream& os, const Channel& channel)
{
	os << "Channel[" << channel.getName() << " users=" << channel.getUsers().size() << "]";
	return os;
}
