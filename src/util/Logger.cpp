#include "Logger.hpp"

Logger::e_level Logger::log_level_ = Logger::LOGLVL_INFO;

LogType Logger::error(isEnabled(LOGLVL_ERROR), "\x1b[1;41m[ERROR]\x1b[0m");
LogType Logger::warn(isEnabled(LOGLVL_WARN), "\x1b[1;43m[WARN]\x1b[0m");
LogType Logger::info(isEnabled(LOGLVL_INFO), "\x1b[1;44m[INFO]\x1b[0m");
LogType Logger::debug(isEnabled(LOGLVL_DEBUG), "\x1b[2;35m[DEBUG]\x1b[0m");
LogType Logger::verbose(isEnabled(LOGLVL_VERBOSE), "\x1b[2;36m[VERBOSE]\x1b[0m");

Logger::Logger(void)
{
	// does nothing in here
}

Logger::Logger(const Logger & other)
{
	// nothing to do here
	(void)other;
}

Logger & Logger::operator=(const Logger & other)
{
	// nothing to do here
	(void)other;
	return (*this);
}

Logger::~Logger(void)
{
	// does nothing in here
}

void Logger::setLogLevel(e_level level)
{
	log_level_ = level;

	error.setEnabled(isEnabled(LOGLVL_ERROR));
	warn.setEnabled(isEnabled(LOGLVL_WARN));
	info.setEnabled(isEnabled(LOGLVL_INFO));
	debug.setEnabled(isEnabled(LOGLVL_DEBUG));
	verbose.setEnabled(isEnabled(LOGLVL_VERBOSE));
}

bool Logger::isEnabled(e_level level)
{
	return (level <= log_level_);
}
