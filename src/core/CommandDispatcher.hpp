#ifndef COMMANDDISPATCHER_HPP
# define COMMANDDISPATCHER_HPP

#include "../networking/Client.hpp"
#include "../messages/Message.hpp"
#include <map>
#include <string>

class NetworkHost;

class CommandDispatcher
{
public:
	explicit CommandDispatcher(const std::string& serverPassword);
	~CommandDispatcher();

	void dispatch(Client& client, const Message& msg);

private:
	typedef void (CommandDispatcher::*CommandHandler)(Client&, const Message&);

	std::string _serverPassword;
	std::map<std::string, CommandHandler> _handlers;

	CommandDispatcher(const CommandDispatcher&);
	CommandDispatcher& operator=(const CommandDispatcher&);

	void handleCap(Client& client, const Message& msg);
	void handlePass(Client& client, const Message& msg);
	void handleNick(Client& client, const Message& msg);
	void handleUser(Client& client, const Message& msg);
	void handleQuit(Client& client, const Message& msg);
	void handleJoin(Client& client, const Message& msg);
	void handlePart(Client& client, const Message& msg);
	void handlePrivmsg(Client& client, const Message& msg);
	void handleMode(Client& client, const Message& msg);
	void handleTopic(Client& client, const Message& msg);
	void handleInvite(Client& client, const Message& msg);
	void handleKick(Client& client, const Message& msg);
	void handlePing(Client& client, const Message& msg);
	void handleNotice(Client& client, const Message& msg);

	void reply(Client& client, const std::string& message);
	void tryCompleteRegistration(Client& client);
};

#endif

void handleClientLeave(Client *client);
void handleClientRecv(Client *client, const std::string &data);
