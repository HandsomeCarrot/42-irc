#include "NetworkHost.hpp"
#include "Client.hpp"
#include "Channel.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

// Forward decls from command layer (defined in core/CommandDispatcher.cpp).
void handleClientLeave(Client *client);
void handleClientRecv(Client *client, const std::string &data);

const unsigned short NetworkHost::defaultPort = 65505;
const std::string NetworkHost::name = "ft_irc.42";
short NetworkHost::pollTimeout = 1000;

NetworkHost::NetworkHost()
	: m_port(defaultPort)
	, m_password()
	, m_open(false)
	, m_listenFd(-1)
	, m_listenReady(false)
{
}

NetworkHost::~NetworkHost()
{
	close();
}

void NetworkHost::setPort(int port)
{
	if (m_open) {
		return;
	}
	m_port = port;
}

void NetworkHost::setPassword(const std::string& password)
{
	if (m_open) {
		return;
	}
	m_password = password;
}


bool NetworkHost::setNonBlocking(int fd)
{
	return ::fcntl(fd, F_SETFL, O_NONBLOCK) == 0;
}

ConnStatus NetworkHost::open()
{
	if (m_open) {
		return CONN_ERR_ALREADY_OPEN;
	}

	int fd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return CONN_ERR_SOCKET;
	}

	int yes = 1;
	if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
		::close(fd);
		return CONN_ERR_SOCKET;
	}

	if (!setNonBlocking(fd)) {
		::close(fd);
		return CONN_ERR_SOCKET;
	}

	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(static_cast<uint16_t>(m_port));

	if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
		::close(fd);
		return CONN_ERR_BIND;
	}
	if (::listen(fd, 16) != 0) {
		::close(fd);
		return CONN_ERR_LISTEN;
	}

	m_listenFd = fd;
	m_open = true;
	m_listenReady = false;
	return CONN_OK;
}

bool NetworkHost::isOpen() const
{
	return m_open;
}

void NetworkHost::clearOwned()
{
	for (std::map<int, Client*>::iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		delete it->second;
	}
	m_clientsByFd.clear();
	for (std::list<Channel*>::iterator it = m_channels.begin();
		 it != m_channels.end(); ++it) {
		delete *it;
	}
	m_channels.clear();
}

void NetworkHost::close()
{
	if (!m_open && m_listenFd < 0 && m_clientsByFd.empty() && m_channels.empty()) {
		return;
	}
	clearOwned();
	if (m_listenFd >= 0) {
		::close(m_listenFd);
		m_listenFd = -1;
	}
	m_open = false;
	m_listenReady = false;
}

int NetworkHost::poll()
{
	if (!m_open || m_listenFd < 0) {
		return -1;
	}

	std::vector<struct pollfd> pfds;
	struct pollfd listenPfd;
	std::memset(&listenPfd, 0, sizeof(listenPfd));
	listenPfd.fd = m_listenFd;
	listenPfd.events = POLLIN;
	pfds.push_back(listenPfd);

	for (std::map<int, Client*>::iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		Client* client = it->second;
		if (client == 0 || client->isMarkedForDeath() || !client->isConnected()) {
			continue;
		}
		struct pollfd pfd;
		std::memset(&pfd, 0, sizeof(pfd));
		pfd.fd = client->getSocket();
		pfd.events = POLLIN;
		if (client->hasPendingOut()) {
			pfd.events = static_cast<short>(pfd.events | POLLOUT);
		}
		pfds.push_back(pfd);
	}

	int rc = ::poll(&pfds[0], static_cast<nfds_t>(pfds.size()), pollTimeout);
	if (rc < 0) {
		if (errno == EINTR) {
			return 0;
		}
		return -1;
	}
	if (rc == 0) {
		return 0;
	}

	for (std::size_t i = 0; i < pfds.size(); ++i) {
		const struct pollfd& pfd = pfds[i];
		if (pfd.revents == 0) {
			continue;
		}
		if (pfd.fd == m_listenFd) {
			if (pfd.revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
				m_listenReady = true;
			}
			continue;
		}
		std::map<int, Client*>::iterator found = m_clientsByFd.find(pfd.fd);
		if (found == m_clientsByFd.end() || found->second == 0) {
			continue;
		}
		short bits = 0;
		if (pfd.revents & POLLIN)
			bits = static_cast<short>(bits + 0x1);
		if (pfd.revents & POLLOUT)
			bits = static_cast<short>(bits + 0x4);
		if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
			bits = static_cast<short>(bits + 0x8);
		found->second->setEvents(bits);
	}
	return rc;
}

void NetworkHost::acceptPending()
{
	if (!m_open || m_listenFd < 0 || !m_listenReady) {
		return;
	}
	for (;;) {
		struct sockaddr_storage ss;
		std::memset(&ss, 0, sizeof(ss));
		socklen_t len = sizeof(ss);
		int cfd = ::accept(m_listenFd, reinterpret_cast<struct sockaddr*>(&ss), &len);
		if (cfd < 0) {
			break;
		}
		if (!setNonBlocking(cfd)) {
			::close(cfd);
			continue;
		}
		int nodelay = 1;
		(void)::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
		if (m_clientsByFd.find(cfd) != m_clientsByFd.end()) {
			::close(cfd);
			continue;
		}
		Client* client = new Client(*this);
		client->connect(cfd);
		client->addr = ss;
		client->addrlen = len;
		if (m_password.empty()) {
			client->regState = STATE_AWAITING_REGISTRATION;
			client->passReceived = true;
		}
		if (!client->isConnected()) {
			delete client;
			continue;
		}
		m_clientsByFd[cfd] = client;
	}
	m_listenReady = false;
}

void NetworkHost::recvReady()
{
	for (std::map<int, Client*>::iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		Client* client = it->second;
		if (client == 0 || client->isMarkedForDeath()) {
			continue;
		}
		if (client->hasError() && !client->canRecv()) {
			handleClientLeave(client);
			continue;
		}
		if (!client->canRecv()) {
			continue;
		}
		ssize_t n = client->recv();
		if (n == 0) {
			handleClientLeave(client);
			continue;
		}
		if (n == -2) {
			continue;
		}
		if (n < 0) {
			handleClientLeave(client);
			continue;
		}
		while (client->hasCompleteIncoming()) {
			std::string line = client->getIncoming();
			if (!line.empty()) {
				handleClientRecv(client, line);
			}
		}
	}
}

void NetworkHost::sendPending()
{
	for (std::map<int, Client*>::iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		Client* client = it->second;
		if (client == 0 || client->isMarkedForDeath() || !client->hasPendingOut()) {
			continue;
		}
		if (client->hasError()) {
			handleClientLeave(client);
			continue;
		}
		ssize_t n = client->send();
		if (n < 0 && n != -2) {
			handleClientLeave(client);
		}
	}
}

void NetworkHost::reapDead()
{
	std::vector<int> dead;
	for (std::map<int, Client*>::iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		if (it->second == 0 || it->second->isMarkedForDeath()) {
			dead.push_back(it->first);
		}
	}
	for (std::size_t i = 0; i < dead.size(); ++i) {
		std::map<int, Client*>::iterator it = m_clientsByFd.find(dead[i]);
		if (it == m_clientsByFd.end()) {
			continue;
		}
		delete it->second;
		m_clientsByFd.erase(it);
	}
}

void NetworkHost::updateClients()
{
	if (!m_open) {
		return;
	}
	acceptPending();
	recvReady();
	sendPending();
	reapDead();
}

std::string NetworkHost::ircLower(const std::string& value)
{
	std::string out;
	out.reserve(value.size());
	for (std::size_t i = 0; i < value.size(); ++i) {
		unsigned char c = static_cast<unsigned char>(value[i]);
		if (c >= 'A' && c <= 'Z') {
			out.push_back(static_cast<char>(c - 'A' + 'a'));
		} else if (c == '[') {
			out.push_back('{');
		} else if (c == ']') {
			out.push_back('}');
		} else if (c == '\\') {
			out.push_back('|');
		} else if (c == '~') {
			out.push_back('`');
		} else {
			out.push_back(static_cast<char>(c));
		}
	}
	return out;
}

Client* NetworkHost::getClient(const std::string& nick)
{
	const std::string want = ircLower(nick);
	for (std::map<int, Client*>::iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		Client* client = it->second;
		if (client == 0 || client->isMarkedForDeath()) {
			continue;
		}
		if (ircLower(client->nick) == want) {
			return client;
		}
	}
	return 0;
}

std::vector<Client*> NetworkHost::getClients() const
{
	std::vector<Client*> out;
	for (std::map<int, Client*>::const_iterator it = m_clientsByFd.begin();
		 it != m_clientsByFd.end(); ++it) {
		if (it->second != 0) {
			out.push_back(it->second);
		}
	}
	return out;
}

void NetworkHost::enqueueSend(Client* client, const std::string& data)
{
	if (client == 0 || client->isMarkedForDeath() || data.empty()) {
		return;
	}
	client->queueSend(data);
}

void NetworkHost::addChannel(Channel* channel)
{
	if (channel == 0) {
		return;
	}
	m_channels.push_back(channel);
}

const std::list<Channel*>& NetworkHost::getChannels() const
{
	return m_channels;
}

std::list<Channel*>& NetworkHost::getChannels()
{
	return m_channels;
}

Channel* NetworkHost::getChannel(const std::string& channelName)
{
	for (std::list<Channel*>::iterator it = m_channels.begin(); it != m_channels.end(); ++it) {
		if (*it != 0 && (*it)->getName() == channelName) {
			return *it;
		}
	}
	return 0;
}

void NetworkHost::rmChannel(Channel* channel)
{
	if (channel == 0) {
		return;
	}
	for (std::list<Channel*>::iterator it = m_channels.begin(); it != m_channels.end(); ++it) {
		if (*it == channel) {
			delete *it;
			m_channels.erase(it);
			return;
		}
	}
}

void NetworkHost::rmChannel(const std::string& channelName)
{
	rmChannel(getChannel(channelName));
}

NetworkHost& NetworkHost::GetGlobal()
{
	return g_host;
}
