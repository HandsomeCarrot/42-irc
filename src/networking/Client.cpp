#include "Client.hpp"
#include "NetworkHost.hpp"
#include "../util/Logger.hpp"


#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

Client::Client()
	: addrlen(0)
	, regState(STATE_AWAITING_PASS)
	, registered(false)
	, passReceived(false)
	, nickReceived(false)
	, userReceived(false)
	, capNegotiating(false)
	, m_host(&NetworkHost::GetGlobal())
	, m_fd(-1)
	, m_connected(false)
	, m_markedForDeath(false)
	, m_events(0)
{
	std::memset(&addr, 0, sizeof(addr));
}

Client::Client(NetworkHost& host)
	: addrlen(0)
	, regState(STATE_AWAITING_PASS)
	, registered(false)
	, passReceived(false)
	, nickReceived(false)
	, userReceived(false)
	, capNegotiating(false)
	, m_host(&host)
	, m_fd(-1)
	, m_connected(false)
	, m_markedForDeath(false)
	, m_events(0)
{
	std::memset(&addr, 0, sizeof(addr));
}

Client::~Client()
{
	disconnect();
}

void Client::connect(int fd)
{
	if (m_markedForDeath || m_connected || fd < 0) {
		return;
	}
	m_fd = fd;
	m_connected = true;
}

void Client::disconnect()
{
	if (!m_connected) {
		return;
	}
	if (m_fd >= 0) {
		::close(m_fd);
		m_fd = -1;
	}
	m_connected = false;
	m_events = 0;
}

bool Client::isConnected() const
{
	return m_connected && m_fd >= 0;
}

int Client::getSocket() const
{
	return m_fd;
}

void Client::queueSend(const std::string& data)
{
	if (!data.empty() && !m_markedForDeath) {
		m_outBuf.append(data);
	}
}

bool Client::hasPendingOut() const
{
	return !m_outBuf.empty();
}

ssize_t Client::send()
{
	if (m_markedForDeath) {
		return 0;
	}
	if (!m_connected || m_fd < 0) {
		return -2;
	}
	if (m_outBuf.empty()) {
		return 0;
	}
	// Readiness comes from poll (POLLOUT when hasPendingOut). Do not branch on errno
	// after send — 42 subject: grade 0 if EAGAIN drives the next step.
	ssize_t n = ::send(m_fd, m_outBuf.data(), m_outBuf.size(), 0);
	if (n < 0) {
		return 0;
	}
	if (n > 0) {
		m_outBuf.erase(0, static_cast<std::string::size_type>(n));
	}
	return n;
}

ssize_t Client::sendNow(const std::string& data)
{
	if (m_markedForDeath || !m_connected || m_fd < 0) {
		return -1;
	}
	if (data.empty()) {
		return 0;
	}
	ssize_t n = ::send(m_fd, data.data(), data.size(), 0);
	if (n < 0) {
		return 0;
	}
	return n;
}

bool Client::hasCompleteIncoming() const
{
	return m_inBuf.find("\r\n") != std::string::npos;
}

std::string Client::getIncoming()
{
	std::string::size_type pos = m_inBuf.find("\r\n");
	if (pos == std::string::npos) {
		return std::string();
	}
	std::string line = m_inBuf.substr(0, pos + 2);
	m_inBuf.erase(0, pos + 2);
	return line;
}

ssize_t Client::recv()
{
	if (m_markedForDeath) {
		return -1;
	}
	if (!m_connected || m_fd < 0) {
		return -1;
	}
	if (!canRecv()) {
		return -2;
	}
	char buf[CLIENT_RECVSIZE];
	// Only called when poll set POLLIN (canRecv). Do not inspect errno after recv.
	ssize_t n = ::recv(m_fd, buf, sizeof(buf), 0);
	Logger::debug << "received " << n << " bytes from " << *this << std::endl;
	m_events = static_cast<short>(m_events & ~static_cast<short>(0x1));
	if (n < 0) {
		return -2;
	}
	if (n == 0) {
		return 0;
	}
	m_inBuf.append(buf, static_cast<std::string::size_type>(n));
	return n;
}

bool Client::canRecv() const
{
	return (m_events & 0x1) != 0;
}

bool Client::canSend() const
{
	return (m_events & 0x4) != 0;
}

bool Client::hasError() const
{
	return (m_events & 0x8) != 0;
}

void Client::setEvents(short events)
{
	m_events = events;
}

std::string Client::mask() const
{
	const std::string& host = (m_host != 0) ? m_host->name : NetworkHost::name;
	std::string n = nick.empty() ? "*" : nick;
	std::string u = user.empty() ? "*" : user;
	return n + "!" + u + "@" + host;
}

void Client::markForDeath()
{
	m_markedForDeath = true;
}

bool Client::isMarkedForDeath() const
{
	return m_markedForDeath;
}

std::ostream& operator<<(std::ostream& os, const Client& client)
{
	os << "Client[fd=" << client.getSocket()
	   << " nick=" << client.nick
	   << " connected=" << (client.isConnected() ? "yes" : "no")
	   << "]";
	return os;
}
