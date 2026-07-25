#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ostream>

#ifndef CLIENT_RECVSIZE
#define CLIENT_RECVSIZE 512
#endif

class NetworkHost;


enum RegistrationState {
	STATE_AWAITING_PASS,
	STATE_AWAITING_REGISTRATION,
	STATE_REGISTERED
};

class Client {
public:
	struct sockaddr_storage addr;
	socklen_t addrlen;
	std::string nick;
	std::string user;
	std::string realname;
	RegistrationState regState;
	bool registered;
	bool passReceived;
	bool nickReceived;
	bool userReceived;
	bool capNegotiating;

	Client();
	explicit Client(NetworkHost& host);
	~Client();

	void connect(int fd);
	void disconnect();
	bool isConnected() const;
	int getSocket() const;

	void queueSend(const std::string& data);
	bool hasPendingOut() const;
	ssize_t send();
	ssize_t sendNow(const std::string& data);

	bool hasCompleteIncoming() const;
	std::string getIncoming();
	ssize_t recv();

	bool canRecv() const;
	bool canSend() const;
	bool hasError() const;
	void setEvents(short events);

	std::string mask() const;
	void markForDeath();
	bool isMarkedForDeath() const;

private:
	Client(const Client&);
	Client& operator=(const Client&);

	NetworkHost* m_host;
	int m_fd;
	bool m_connected;
	bool m_markedForDeath;
	short m_events;
	std::string m_inBuf;
	std::string m_outBuf;
};

std::ostream& operator<<(std::ostream& os, const Client& client);

#endif
