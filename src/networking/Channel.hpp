#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <set>
#include <ostream>

class Client;
class NetworkHost;

class Channel {
public:
	enum Type {
		TINVAL = '\0',
		REGULAR = '#',
		LOCAL = '&',
		NOMODE = '+',
		IOWN = '!'
	};

	enum Mode {
		MINVAL = '\0',
		INVITEONLY = 'i',
		PROTECTEDTOPIC = 't',
		KEY = 'k',
		OPERATOR = 'o',
		USERLIMIT = 'l'
	};

	explicit Channel(const std::string& channelName);
	Channel(const std::string& channelName, NetworkHost& host);
	~Channel();

	static bool isValidName(const std::string& channelName);

	std::map<int, Client*>& getUsers();
	const std::map<int, Client*>& getUsers() const;
	Client* getUserByNick(const std::string& nick) const;
	bool hasUser(int fd) const;
	void addUser(Client* client);
	void rmUser(Client* client, const std::string& notifyLine = std::string());
	void rmUser(int fd, const std::string& notifyLine = std::string());
	void broadcast(const std::string& message, Client* except = 0);

	const std::string& getName() const;
	Type getPrefix() const;
	const std::string& getTopic() const;
	void setTopic(const std::string& topic);

	void setMode(Mode mode, bool enabled);
	bool isModeSet(Mode mode) const;

	bool isOperator(Client* client) const;
	void addOperator(Client* client);
	void rmOperator(Client* client);

	void invite(Client* client);
	bool isInvited(Client* client) const;

	void setKey(const std::string& key);
	const std::string& getKey() const;
	void clearKey();

	int getUserLimit() const;
	void setUserLimit(int limit);
	void clearUserLimit();

	void markForDeath();
	bool isMarkedForDeath() const;

private:
	Channel(const Channel&);
	Channel& operator=(const Channel&);

	static std::string ircLower(const std::string& value);
	static Type prefixType(const std::string& channelName);
	void removeMember(int fd, const std::string& notifyLine);
	void initFromName(const std::string& channelName);

	NetworkHost* m_host;
	std::string m_name;
	Type m_type;
	std::string m_topic;
	int m_modes;
	std::string m_key;
	int m_userLimit;
	bool m_markedForDeath;
	std::map<int, Client*> m_users;
	std::set<int> m_operators;
	std::set<int> m_invites;
};

std::ostream& operator<<(std::ostream& os, const Channel& channel);

#endif
