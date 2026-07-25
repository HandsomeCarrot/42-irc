#ifndef NETWORK_HOST_HPP
#define NETWORK_HOST_HPP

#include "conn_status.hpp"

#include <string>
#include <vector>
#include <map>
#include <list>

#ifndef PROTO_TCP
#define PROTO_TCP 6
#endif

#ifndef MAX_POLLEVENTS
#define MAX_POLLEVENTS 64
#endif

class Client;
class Channel;

class NetworkHost {
public:
	static const unsigned short defaultPort;
	static const std::string name;
	static short pollTimeout;

	NetworkHost();
	~NetworkHost();

	void setPort(int port);
	void setPassword(const std::string& password);

	ConnStatus open();
	bool isOpen() const;
	void close();

	int poll();
	void updateClients();

	Client* getClient(const std::string& nick);
	std::vector<Client*> getClients() const;
	void enqueueSend(Client* client, const std::string& data);

	void addChannel(Channel* channel);
	const std::list<Channel*>& getChannels() const;
	std::list<Channel*>& getChannels();
	Channel* getChannel(const std::string& channelName);
	void rmChannel(Channel* channel);
	void rmChannel(const std::string& channelName);

	static NetworkHost& GetGlobal();

private:
	NetworkHost(const NetworkHost&);
	NetworkHost& operator=(const NetworkHost&);

	void clearOwned();
	void acceptPending();
	void recvReady();
	void sendPending();
	void reapDead();
	static std::string ircLower(const std::string& value);
	static bool setNonBlocking(int fd);

	int m_port;
	std::string m_password;
	bool m_open;
	int m_listenFd;
	bool m_listenReady;
	std::map<int, Client*> m_clientsByFd;
	std::list<Channel*> m_channels;
};

extern NetworkHost g_host;

#endif
