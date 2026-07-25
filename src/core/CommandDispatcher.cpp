#include "CommandDispatcher.hpp"
#include "../networking/Client.hpp"
#include "../networking/NetworkHost.hpp"
#include "../networking/Channel.hpp"
#include "Replies.hpp"
#include "../util/Logger.hpp"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <cctype>
#include <set>

static CommandDispatcher *g_dispatcher = NULL;

namespace
{
	void purgeClientFromChannels(Client *client, const std::string &quitLine);
}


void handleClientLeave(Client *client)
{
	if (!client)
		return;

	Logger::info << "Client leaving: " << *client << std::endl;
	std::string quitLine = ":" + client->mask() + " QUIT :Connection closed\r\n";
	purgeClientFromChannels(client, quitLine);

	if (client->isConnected()) {
		try {
			client->disconnect();
		} catch (std::exception &e) {
			Logger::warn << "Error disconnecting leaving client: " << e.what() << std::endl;
		}
	}
	client->markForDeath();
}

void handleClientRecv(Client *client, const std::string &data)
{
	if (!client || !g_dispatcher) {
		Logger::error << "handleClientRecv called with NULL client or dispatcher" << std::endl;
		return;
	}

	try
	{
		Message msg(data, client->getSocket());
		g_dispatcher->dispatch(*client, msg);
	}
	catch (const std::exception &e)
	{
		Logger::warn << "Invalid message from " << *client << ": " << e.what() << std::endl;
	}
}
namespace
{
	bool isChannelName(const std::string& name)
	{
		if (name.empty())
			return (false);
		char c = name[0];
		return (c == '#' || c == '&' || c == '+' || c == '!');
	}

	bool isValidNick(const std::string& nick)
	{
		if (nick.empty() || nick.size() > 9)
			return (false);
		const std::string special = "[]\\`_^{|}";
		if (!std::isalpha(static_cast<unsigned char>(nick[0]))
			&& special.find(nick[0]) == std::string::npos)
			return (false);
		for (std::string::size_type i = 1; i < nick.size(); ++i)
		{
			char c = nick[i];
			if (!std::isalnum(static_cast<unsigned char>(c))
				&& c != '-' && special.find(c) == std::string::npos)
				return (false);
		}
		return (true);
	}
}

CommandDispatcher::CommandDispatcher(const std::string& serverPassword)
: _serverPassword(serverPassword)
{
	g_dispatcher = this;

	_handlers["CAP"]     = &CommandDispatcher::handleCap;
	_handlers["PING"]    = &CommandDispatcher::handlePing;
	_handlers["PASS"]    = &CommandDispatcher::handlePass;
	_handlers["NICK"]    = &CommandDispatcher::handleNick;
	_handlers["USER"]    = &CommandDispatcher::handleUser;
	_handlers["QUIT"]    = &CommandDispatcher::handleQuit;
	_handlers["JOIN"]    = &CommandDispatcher::handleJoin;
	_handlers["PART"]    = &CommandDispatcher::handlePart;
	_handlers["PRIVMSG"] = &CommandDispatcher::handlePrivmsg;
	_handlers["NOTICE"]  = &CommandDispatcher::handleNotice;
	_handlers["MODE"]    = &CommandDispatcher::handleMode;
	_handlers["TOPIC"]   = &CommandDispatcher::handleTopic;
	_handlers["INVITE"]  = &CommandDispatcher::handleInvite;
	_handlers["KICK"]    = &CommandDispatcher::handleKick;
}

CommandDispatcher::~CommandDispatcher()
{
	g_dispatcher = NULL;
}


namespace {

// Remove client from every channel that still holds fd in the raw user map.
// Uses the map (not hasUser) so cleanup still works if markForDeath already ran.
void purgeClientFromChannels(Client *client, const std::string &quitLine)
{
	if (!client)
		return;
	const int clientFd = client->getSocket();
	std::list<Channel*> channelsToCleanup;
	const std::list<Channel*> &allChannels = g_host.getChannels();
	for (std::list<Channel*>::const_iterator it = allChannels.begin();
		 it != allChannels.end(); ++it)
	{
		if ((*it)->getUsers().find(clientFd) != (*it)->getUsers().end())
			channelsToCleanup.push_back(*it);
	}
	for (std::list<Channel*>::iterator it = channelsToCleanup.begin();
		 it != channelsToCleanup.end(); ++it)
	{
		Channel *ch = *it;
		ch->rmUser(clientFd);
		if (!quitLine.empty())
			ch->broadcast(quitLine);
		if (ch->getUsers().empty()) {
			Logger::info << "Removing empty channel " << *ch << std::endl;
			g_host.rmChannel(ch);
		}
	}
}

} // namespace

void CommandDispatcher::dispatch(Client& client, const Message& msg)
{
	std::string command = msg.getCommand();
	std::string target = client.nick.empty() ? "*" : client.nick;

	// CAP / QUIT always; PING pre-reg so clients don't time out mid-handshake.
	if (command == "CAP" || command == "QUIT" || command == "PING")
	{
		std::map<std::string, CommandHandler>::iterator always = _handlers.find(command);
		if (always != _handlers.end())
		{
			Logger::debug << "Dispatching " << command << " from " << client << std::endl;
			(this->*(always->second))(client, msg);
		}
		return;
	}

	if (client.regState == STATE_AWAITING_PASS)
	{
		if (command == "PASS")
		{
			Logger::debug << "Dispatching PASS from " << client << std::endl;
			handlePass(client, msg);
		}
		else
			reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	if (client.regState == STATE_AWAITING_REGISTRATION)
	{
		if (command == "PASS")
		{
			// Already authenticated (or passwordless). Re-PASS after success → 462 when password set.
			if (!_serverPassword.empty())
				reply(client, ERR_ALREADYREGISTRED(target, NetworkHost::name));
			else
			{
				Logger::debug << "Dispatching PASS from " << client << std::endl;
				handlePass(client, msg);
			}
			return;
		}
		if (command == "NICK" || command == "USER")
		{
			std::map<std::string, CommandHandler>::iterator it = _handlers.find(command);
			if (it != _handlers.end())
			{
				Logger::debug << "Dispatching " << command << " from " << client << std::endl;
				(this->*(it->second))(client, msg);
			}
			return;
		}
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	// STATE_REGISTERED
	if (command == "PASS" || command == "USER")
	{
		reply(client, ERR_ALREADYREGISTRED(target, NetworkHost::name));
		return;
	}

	std::map<std::string, CommandHandler>::iterator it = _handlers.find(command);
	if (it != _handlers.end())
	{
		Logger::debug << "Dispatching " << command << " from " << client << std::endl;
		(this->*(it->second))(client, msg);
	}
	else
	{
		Logger::warn << "Unknown command \"" << command << "\" from " << client << std::endl;
		reply(client, ERR_NOSUCHCOMMAND(target, NetworkHost::name, command));
	}
}

void CommandDispatcher::tryCompleteRegistration(Client& client)
{
	if (client.regState == STATE_REGISTERED || client.registered)
		return;
	if (client.regState != STATE_AWAITING_REGISTRATION)
		return;
	if (client.nickReceived && client.userReceived && !client.capNegotiating)
	{
		client.regState = STATE_REGISTERED;
		client.registered = true;
		Logger::info << "Client registered: " << client << std::endl;
		reply(client, RPL_WELCOME(client.nick, client.user, NetworkHost::name));
		reply(client, RPL_YOURHOST(client.nick, NetworkHost::name));
		reply(client, RPL_CREATED(client.nick, NetworkHost::name, "today"));
		reply(client, RPL_MYINFO(client.nick, NetworkHost::name));
	}
}

void CommandDispatcher::handlePing(Client& client, const Message& msg)
{
	std::string token = msg.getParams().empty() ? "server" : msg.getParams()[0];
	std::string pong = ":" + NetworkHost::name + " PONG " + NetworkHost::name + " :" + token;
	reply(client, pong);
}

void CommandDispatcher::handleCap(Client& client, const Message& msg)
{
	const Message::t_params &params = msg.getParams();
	std::string subcommand = params.empty() ? "" : params[0];
	for (std::string::size_type i = 0; i < subcommand.size(); ++i)
		subcommand[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(subcommand[i])));

	if (!client.registered)
		client.capNegotiating = true;

	if (subcommand == "LS")
	{
		reply(client, ":" + NetworkHost::name + " CAP * LS :");
	}
	else if (subcommand == "LIST")
	{
		reply(client, ":" + NetworkHost::name + " CAP * LIST :");
	}
	else if (subcommand == "REQ")
	{
		std::string capParam = (params.size() > 1) ? params[1] : "";
		reply(client, ":" + NetworkHost::name + " CAP * NAK :" + capParam);
	}
	else if (subcommand == "END")
	{
		client.capNegotiating = false;
		tryCompleteRegistration(client);
	}
}

void CommandDispatcher::handlePass(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;

	if (client.regState == STATE_REGISTERED || client.registered)
	{
		reply(client, ERR_ALREADYREGISTRED(target, NetworkHost::name));
		return;
	}
	if (msg.getParams().empty())
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "PASS"));
		return;
	}
	if (!_serverPassword.empty() && msg.getParams()[0] != _serverPassword)
	{
		Logger::warn << "Password mismatch from " << client << std::endl;
		// Flush 464 before tear-down; queued reply may never leave the socket.
		client.sendNow(ERR_PASSWDMISMATCH(target, NetworkHost::name) + "\r\n");
		handleClientLeave(&client);
		return;
	}
	client.passReceived = true;
	client.regState = STATE_AWAITING_REGISTRATION;
	Logger::debug << "Password accepted for " << client << std::endl;
	tryCompleteRegistration(client);
}

void CommandDispatcher::handleNick(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;

	if (msg.getParams().empty())
	{
		reply(client, ERR_NONICKNAMEGIVEN(target, NetworkHost::name));
		return;
	}

	std::string newNick = msg.getParams()[0];

	if (!isValidNick(newNick))
	{
		Logger::debug << "Invalid nick \"" << newNick << "\" from " << client << std::endl;
		reply(client, ERR_ERRONEUSNICKNAME(target, NetworkHost::name, newNick));
		return;
	}
	Client* existing = g_host.getClient(newNick);
	if (existing && existing != &client)
	{
		Logger::debug << "Nick \"" << newNick << "\" already in use (from " << client << ")" << std::endl;
		reply(client, ERR_NICKNAMEINUSE(target, NetworkHost::name, newNick));
		return;
	}

	std::string oldMask = client.mask();
	std::string oldNick = client.nick;
	client.nick = newNick;
	client.nickReceived = true;

	if (client.registered)
	{
		Logger::info << "Nick change: " << oldNick << " -> " << newNick
			<< " (" << client << ")" << std::endl;
		std::string nickLine = ":" + oldMask + " NICK " + newNick;
		const int selfFd = client.getSocket();
		std::set<int> notified;
		notified.insert(selfFd);
		const std::list<Channel*> &channels = g_host.getChannels();
		for (std::list<Channel*>::const_iterator it = channels.begin();
			it != channels.end(); ++it)
		{
			if (!(*it)->hasUser(selfFd))
				continue;
			const std::map<int, Client*> &users = (*it)->getUsers();
			for (std::map<int, Client*>::const_iterator uit = users.begin();
				uit != users.end(); ++uit)
			{
				if (uit->second && notified.insert(uit->first).second)
					reply(*uit->second, nickLine);
			}
		}
		reply(client, nickLine);
	}
	else
	{
		Logger::debug << "Nick set to \"" << newNick << "\" for " << client << std::endl;
	}
	tryCompleteRegistration(client);
}

void CommandDispatcher::handleUser(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;

	if (client.registered)
	{
		reply(client, ERR_ALREADYREGISTRED(target, NetworkHost::name));
		return;
	}
	if (msg.getParams().size() < 4)
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "USER"));
		return;
	}
	client.user = msg.getParams()[0];
	client.realname = msg.getParams()[3];
	client.userReceived = true;
	tryCompleteRegistration(client);
}

void CommandDispatcher::handleQuit(Client& client, const Message& msg)
{
	std::string reason = "Client quit";
	if (!msg.getParams().empty())
		reason = msg.getParams()[0];

	Logger::info << "QUIT from " << client << ": " << reason << std::endl;

	std::string errorLine = ":" + NetworkHost::name + " ERROR :Closing connection: " + reason + "\r\n";
	client.sendNow(errorLine);

	std::string quitLine = ":" + client.mask() + " QUIT :" + reason + "\r\n";
	purgeClientFromChannels(&client, quitLine);
	handleClientLeave(&client);
}

void CommandDispatcher::handleJoin(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.empty())
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "JOIN"));
		return;
	}
	std::string channelName = params[0];
	Channel *channel = g_host.getChannel(channelName);

	if (!channel)
	{
		if (!Channel::isValidName(channelName))
		{
			Logger::warn << "Failed to create channel \"" << channelName
				<< "\": invalid name" << std::endl;
			reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, channelName));
			return;
		}
		channel = new Channel(channelName);
		g_host.addChannel(channel);
		Logger::info << "Created channel " << *channel << " for " << client << std::endl;
	}

	bool isFirstUser = channel->getUsers().empty();

	Channel::Mode inviteMode = Channel::INVITEONLY;
	if (channel->isModeSet(inviteMode) && !channel->isInvited(&client))
	{
		Logger::debug << client << " denied JOIN " << *channel << " (invite-only)" << std::endl;
		reply(client, ERR_INVITEONLYCHAN(target, NetworkHost::name, channelName));
		return;
	}

	Channel::Mode keyMode = Channel::KEY;
	bool keyRequired = channel->isModeSet(keyMode) || !channel->getKey().empty();
	if (keyRequired)
	{
		if (params.size() < 2 || params[1] != channel->getKey())
		{
			Logger::debug << client << " denied JOIN " << *channel << " (bad key)" << std::endl;
			reply(client, ERR_BADCHANNELKEY(target, NetworkHost::name, channelName));
			return;
		}
	}

	Channel::Mode limitMode = Channel::USERLIMIT;
	if (channel->isModeSet(limitMode))
	{
		if (static_cast<int>(channel->getUsers().size()) >= channel->getUserLimit())
		{
			Logger::debug << client << " denied JOIN " << *channel << " (full)" << std::endl;
			reply(client, ERR_CHANNELISFULL(target, NetworkHost::name, channelName));
			return;
		}
	}

	channel->addUser(&client);
	if (isFirstUser)
		channel->addOperator(&client);

	Logger::info << client << " joined " << *channel
		<< (isFirstUser ? " as operator" : "") << std::endl;

	std::string joinLine = ":" + client.mask() + " JOIN " + channel->getName() + "\r\n";
	channel->broadcast(joinLine);

	if (channel->getTopic().empty())
		reply(client, RPL_NOTOPIC(target, NetworkHost::name, channel->getName()));
	else
		reply(client, RPL_TOPIC(target, NetworkHost::name, channel->getName(), channel->getTopic()));

	std::string users;
	for (std::map<int, Client*>::const_iterator uit = channel->getUsers().begin();
		uit != channel->getUsers().end(); ++uit)
	{
		if (!users.empty())
			users += " ";
		if (channel->isOperator(uit->second))
			users += "@";
		users += uit->second->nick;
	}
	reply(client, RPL_NAMREPLY(target, NetworkHost::name, "=", channel->getName(), users));
	reply(client, RPL_ENDOFNAMES(target, NetworkHost::name, channel->getName()));
}

void CommandDispatcher::handlePart(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.empty())
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "PART"));
		return;
	}

	std::string channelName = params[0];
	Channel *channel = g_host.getChannel(channelName);
	if (!channel)
	{
		reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, channelName));
		return;
	}
	if (!channel->hasUser(client.getSocket()))
	{
		reply(client, ERR_NOTONCHANNEL(target, NetworkHost::name, channelName));
		return;
	}

	std::string reason = (params.size() > 1) ? params[1] : "Leaving";
	std::string partLine = ":" + client.mask() + " PART " + channelName + " :" + reason + "\r\n";
	Logger::info << client << " parted " << *channel << ": " << reason << std::endl;
	channel->broadcast(partLine);
	channel->rmUser(&client);
	if (channel->getUsers().empty())
	{
		Logger::info << "Removing empty channel " << *channel << std::endl;
		g_host.rmChannel(channel);
	}
}

void CommandDispatcher::handlePrivmsg(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.empty())
	{
		reply(client, ERR_NORECIPIENT(target, NetworkHost::name, "PRIVMSG"));
		return;
	}
	if (params.size() < 2)
	{
		reply(client, ERR_NOTEXTTOSEND(target, NetworkHost::name));
		return;
	}

	std::string targetName = params[0];
	std::string text = params[1];
	std::string line = ":" + client.mask() + " PRIVMSG " + targetName + " :" + text;

	Logger::debug << client << " PRIVMSG " << targetName << std::endl;

	if (isChannelName(targetName))
	{
		Channel *channel = g_host.getChannel(targetName);
		if (!channel)
		{
			reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, targetName));
			return;
		}
		if (!channel->hasUser(client.getSocket()))
		{
			reply(client, ERR_NOTONCHANNEL(target, NetworkHost::name, targetName));
			return;
		}
		for (std::map<int, Client*>::const_iterator uit = channel->getUsers().begin();
			uit != channel->getUsers().end(); ++uit)
		{
			if (uit->second->getSocket() == client.getSocket())
				continue;
			reply(*uit->second, line);
		}
	}
	else
	{
		Client *targetClient = g_host.getClient(targetName);
		if (!targetClient)
		{
			reply(client, ERR_NOSUCHNICK(target, NetworkHost::name, targetName));
			return;
		}
		reply(*targetClient, line);
	}
}
void CommandDispatcher::handleNotice(Client& client, const Message& msg) // cppcheck-suppress constParameterCallback
{
	if (!client.registered)
	{
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.empty() || params.size() < 2)
	{
		return;
	}

	std::string targetName = params[0];
	std::string text = params[1];
	std::string line = ":" + client.mask() + " NOTICE " + targetName + " :" + text;

	Logger::debug << client << " NOTICE " << targetName << std::endl;

	if (isChannelName(targetName))
	{
		Channel *channel = g_host.getChannel(targetName);
		if (!channel)
			return;  // silent
		if (!channel->hasUser(client.getSocket()))
			return;  // silent
		for (std::map<int, Client*>::const_iterator uit = channel->getUsers().begin();
			uit != channel->getUsers().end(); ++uit)
		{
			if (uit->second->getSocket() == client.getSocket())
				continue;
			reply(*uit->second, line);
		}
	}
	else
	{
		Client *targetClient = g_host.getClient(targetName);
		if (!targetClient)
			return;  // silent — no ERR_NOSUCHNICK
		reply(*targetClient, line);
	}
}

void CommandDispatcher::handleMode(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.empty())
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "MODE"));
		return;
	}

	std::string modeTarget = params[0];

	if (params.size() == 1)
	{
		if (isChannelName(modeTarget))
		{
			Channel *channel = g_host.getChannel(modeTarget);
			if (!channel)
			{
				reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, modeTarget));
				return;
			}
			std::string activeModes = "+";
			std::string modeArgs;
			Channel::Mode m;

			m = Channel::INVITEONLY;
			if (channel->isModeSet(m))
				activeModes += 'i';

			m = Channel::PROTECTEDTOPIC;
			if (channel->isModeSet(m))
				activeModes += 't';

			m = Channel::KEY;
			if (channel->isModeSet(m))
			{
				activeModes += 'k';
				// Key is always the first mode arg when present.
				if (channel->hasUser(client.getSocket()))
					modeArgs = channel->getKey();
			}

			m = Channel::USERLIMIT;
			if (channel->isModeSet(m))
			{
				activeModes += 'l';
				if (!modeArgs.empty())
					modeArgs += " ";
				std::ostringstream oss;
				oss << channel->getUserLimit();
				modeArgs += oss.str();
			}

			reply(client, RPL_CHANNELMODEIS(target, NetworkHost::name, modeTarget, activeModes, modeArgs));
			return;
		}
		else
		{
			if (modeTarget != client.nick)
			{
				reply(client, ERR_USERSDONTMATCH(target, NetworkHost::name));
				return;
			}
			// No user-modes implemented; report empty set.
			reply(client, RPL_UMODEIS(target, NetworkHost::name, "+"));
			return;
		}
	}
	if (!isChannelName(modeTarget))
	{
		if (modeTarget != client.nick)
		{
			reply(client, ERR_USERSDONTMATCH(target, NetworkHost::name));
			return;
		}
		// Ignore umode changes; do not ACK/echo (avoids fake "Mode change [+i]").
		(void)params;
		return;
	}
	Channel *channel = g_host.getChannel(modeTarget);
	if (!channel)
	{
		reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, modeTarget));
		return;
	}
	if (params.size() == 2 && params[1] == "b")
	{
		reply(client, RPL_ENDOFBANLIST(target, NetworkHost::name, modeTarget));
		return;
	}
	if (!channel->isOperator(&client))
	{
		reply(client, ERR_CHANOPRIVSNEEDED(target, NetworkHost::name, modeTarget));
		return;
	}

	std::string modes = params[1];
	if (modes.empty())
		return;

	bool add = true;
	char lastAppliedSign = '\0';
	size_t argIndex = 2;
	std::string outModes;
	std::string outArgs;

	for (size_t i = 0; i < modes.size(); ++i)
	{
		char c = modes[i];

		if (c == '+' || c == '-')
		{
			add = (c == '+');
			continue;
		}

		std::string arg;
		bool applied = false;

		switch (c)
		{
			case 'i':
			{
				Channel::Mode m = Channel::INVITEONLY;
				channel->setMode(m, add);
				applied = true;
				break;
			}
			case 't':
			{
				Channel::Mode m = Channel::PROTECTEDTOPIC;
				channel->setMode(m, add);
				applied = true;
				break;
			}
			case 'k':
			{
				Channel::Mode m = Channel::KEY;
				if (add)
				{
					if (argIndex >= params.size())
					{
						reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "MODE"));
						break;
					}
					arg = params[argIndex++];
					channel->setKey(arg);
					channel->setMode(m, true);
				}
				else
				{
					channel->clearKey();
					channel->setMode(m, false);
				}
				applied = true;
				break;
			}
			case 'l':
			{
				Channel::Mode m = Channel::USERLIMIT;
				if (add)
				{
					if (argIndex >= params.size())
					{
						reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "MODE"));
						break;
					}
					arg = params[argIndex++];
					int limit = std::atoi(arg.c_str());
					if (limit < 0)
						limit = 0;
					channel->setUserLimit(limit);
					channel->setMode(m, true);
				}
				else
				{
					channel->clearUserLimit();
					channel->setMode(m, false);
				}
				applied = true;
				break;
			}
			case 'o':
			{
				if (argIndex >= params.size())
				{
					reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "MODE"));
					break;
				}
				arg = params[argIndex++];
				Client *targetClient = g_host.getClient(arg);
				if (!targetClient)
				{
					reply(client, ERR_NOSUCHNICK(target, NetworkHost::name, arg));
					break;
				}
				if (!channel->hasUser(targetClient->getSocket()))
				{
					reply(client, ERR_USERNOTINCHANNEL(target, NetworkHost::name, arg, modeTarget));
					break;
				}
				if (add)
				{
					channel->addOperator(targetClient);
					Logger::info << "Promoted " << *targetClient << " to operator on " << *channel << std::endl;
				}
				else
				{
					channel->rmOperator(targetClient);
					Logger::info << "Demoted " << *targetClient << " from operator on " << *channel << std::endl;
				}
				applied = true;
				break;
			}
			default:
			{
				std::string charStr(1, c);
				reply(client, ERR_UNKNOWNMODE(target, NetworkHost::name, charStr));
				break;
			}
		}

		if (!applied)
			continue;

		char sign = add ? '+' : '-';
		if (sign != lastAppliedSign)
		{
			outModes += sign;
			lastAppliedSign = sign;
		}
		outModes += c;

		if (!arg.empty())
		{
			if (!outArgs.empty())
				outArgs += " ";
			outArgs += arg;
		}
	}

	if (outModes.empty())
		return;

	std::string modeLine = ":" + client.mask() + " MODE " + modeTarget + " " + outModes;
	if (!outArgs.empty())
		modeLine += " " + outArgs;
	modeLine += "\r\n";
	Logger::info << client << " MODE " << modeTarget << " " << outModes
		<< (outArgs.empty() ? "" : " ") << outArgs << std::endl;
	channel->broadcast(modeLine);
}

void CommandDispatcher::handleTopic(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.empty())
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "TOPIC"));
		return;
	}

	std::string channelName = params[0];
	Channel *channel = g_host.getChannel(channelName);
	if (!channel)
	{
		reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, channelName));
		return;
	}
	if (!channel->hasUser(client.getSocket()))
	{
		reply(client, ERR_NOTONCHANNEL(target, NetworkHost::name, channelName));
		return;
	}

	if (params.size() == 1)
	{
		if (channel->getTopic().empty())
			reply(client, RPL_NOTOPIC(target, NetworkHost::name, channelName));
		else
			reply(client, RPL_TOPIC(target, NetworkHost::name, channelName, channel->getTopic()));
		return;
	}

	Channel::Mode protectedTopic = Channel::PROTECTEDTOPIC;
	if (channel->isModeSet(protectedTopic) && !channel->isOperator(&client))
	{
		reply(client, ERR_CHANOPRIVSNEEDED(target, NetworkHost::name, channelName));
		return;
	}

	std::string newTopic = params[1];
	channel->setTopic(newTopic);
	Logger::info << client << " set topic on " << *channel << ": " << newTopic << std::endl;
	std::string line = ":" + client.mask() + " TOPIC " + channelName + " :" + newTopic + "\r\n";
	channel->broadcast(line);
}

void CommandDispatcher::handleInvite(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.size() < 2)
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "INVITE"));
		return;
	}

	std::string nick = params[0];
	std::string channelName = params[1];

	Channel *channel = g_host.getChannel(channelName);
	if (!channel)
	{
		reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, channelName));
		return;
	}
	if (!channel->hasUser(client.getSocket()))
	{
		reply(client, ERR_NOTONCHANNEL(target, NetworkHost::name, channelName));
		return;
	}

	Channel::Mode inviteOnly = Channel::INVITEONLY;
	if (channel->isModeSet(inviteOnly) && !channel->isOperator(&client))
	{
		reply(client, ERR_CHANOPRIVSNEEDED(target, NetworkHost::name, channelName));
		return;
	}

	Client *targetClient = g_host.getClient(nick);
	if (!targetClient)
	{
		reply(client, ERR_NOSUCHNICK(target, NetworkHost::name, nick));
		return;
	}
	if (channel->hasUser(targetClient->getSocket()))
	{
		reply(client, ERR_USERONCHANNEL(target, NetworkHost::name, nick, channelName));
		return;
	}

	channel->invite(targetClient);
	Logger::info << client << " invited " << *targetClient
		<< " to " << *channel << std::endl;
	reply(*targetClient, ":" + client.mask() + " INVITE " + nick + " " + channelName);
	reply(client, RPL_INVITING(target, NetworkHost::name, nick, channelName));
}

void CommandDispatcher::handleKick(Client& client, const Message& msg)
{
	std::string target = client.nick.empty() ? "*" : client.nick;
	if (!client.registered)
	{
		reply(client, ERR_NOTREGISTERED(target, NetworkHost::name));
		return;
	}

	const Message::t_params &params = msg.getParams();
	if (params.size() < 2)
	{
		reply(client, ERR_NEEDMOREPARAMS(target, NetworkHost::name, "KICK"));
		return;
	}

	std::string channelName = params[0];
	std::string nick = params[1];
	std::string reason = (params.size() > 2) ? params[2] : "Kicked";

	Channel *channel = g_host.getChannel(channelName);
	if (!channel)
	{
		reply(client, ERR_NOSUCHCHANNEL(target, NetworkHost::name, channelName));
		return;
	}
	if (!channel->isOperator(&client))
	{
		reply(client, ERR_CHANOPRIVSNEEDED(target, NetworkHost::name, channelName));
		return;
	}

	Client *targetClient = g_host.getClient(nick);
	if (!targetClient)
	{
		reply(client, ERR_NOSUCHNICK(target, NetworkHost::name, nick));
		return;
	}
	if (!channel->hasUser(targetClient->getSocket()))
	{
		reply(client, ERR_USERNOTINCHANNEL(target, NetworkHost::name, nick, channelName));
		return;
	}

	std::string line = ":" + client.mask() + " KICK " + channelName + " " + nick + " :" + reason + "\r\n";
	Logger::info << client << " kicked " << *targetClient
		<< " from " << *channel << ": " << reason << std::endl;
	g_host.enqueueSend(targetClient, line);
	channel->rmUser(targetClient, line);
	if (channel->getUsers().empty())
	{
		Logger::info << "Removing empty channel " << *channel << std::endl;
		g_host.rmChannel(channel);
	}
}


void CommandDispatcher::reply(Client& client, const std::string& message)
{
	g_host.enqueueSend(&client, message + "\r\n");
}
