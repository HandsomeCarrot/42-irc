#include "networking/NetworkHost.hpp"
#include "./core/CommandDispatcher.hpp"
#include "util/Logger.hpp"
#include <csignal>
#include <cstdlib>

extern NetworkHost g_host;

static bool g_exit = false;

void onSignal(int signal)
{
	if (signal == SIGINT || signal == SIGTERM)
		g_exit = true;
}

int main(int argc, char **argv)
{
	// set to a specific log level, default: info
	//Logger::setLogLevel(Logger::LOGLVL_VERBOSE);
	Logger::info << "Starting server..." << std::endl;

	signal(SIGINT, onSignal);
	signal(SIGTERM, onSignal);
	signal(SIGPIPE, SIG_IGN);

	if (argc >= 2)
	{
		g_host.setPort(atoi(argv[1]));
		Logger::info << "Set host port to " << atoi(argv[1]) << std::endl;
	}
	else
	{
		Logger::warn << "No port specified, using default (" << NetworkHost::defaultPort << ")" << std::endl;
	}

	std::string serverPassword;
	if (argc >= 3)
	{
		serverPassword = argv[2];
		g_host.setPassword(serverPassword);
		Logger::info << "Set password to \"" << serverPassword << "\"" << std::endl;
	}
	else
	{
		Logger::warn << "No password specified, using default (blank)" << std::endl;
	}


	CommandDispatcher dispatcher(serverPassword);

	g_host.open();
	if (!g_host.isOpen())
	{
		Logger::error << "Could not open host" << std::endl;
		return (0);
	}

	Logger::info << "Server started!" << std::endl;

	while (!g_exit)
	{
		int pollret = g_host.poll();
		if (pollret == -1) {
			Logger::warn << "Poll error" << std::endl;
			continue;
		}
		if (pollret == 0) {
			continue;
		}
		g_host.updateClients();
	}

	g_host.close();
	Logger::info << "Server stopped!" << std::endl;
	return (0);
}
