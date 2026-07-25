#include "LogType.hpp"
#include <ctime>

std::ostream & LogType::target_stream_ = std::cerr;

LogType::LogType(void)
	: enabled_(false)
	, line_start_(true)
	, prefix_("")
{
	// nothing in here
}

LogType::LogType(bool enabled, const std::string & prefix)
	: enabled_(enabled)
	, line_start_(true)
	, prefix_(prefix)
{
	// nothing to do here
}

LogType::LogType(const LogType & other)
	: enabled_(other.enabled_)
	, line_start_(other.line_start_)
	, prefix_(other.prefix_)
{
	// nothing in here
}

LogType & LogType::operator=(const LogType & other)
{
	if (this == &other)
		return (*this);

	this->enabled_ = other.enabled_;
	this->line_start_ = other.line_start_;
	this->prefix_ = other.prefix_;

	return (*this);
}

LogType::~LogType(void)
{
	// nothing in here
}

void LogType::setEnabled(bool enable)
{
	enabled_ = enable;
}

LogType & LogType::operator<<(std::ostream & (*function)(std::ostream &))
{
	if (!enabled_)
		return (*this);

	target_stream_ << function;
	line_start_ = true;

	return (*this);
}

void LogType::writePrefix(void)
{
	if (!line_start_)
		return ;

	line_start_ = false;

	char time_string[22];
	std::time_t now = std::time(NULL);
	std::strftime(time_string, sizeof(time_string), "[%Y-%m-%dT%H-%M-%S]", std::localtime(&now));

	target_stream_ << time_string << ' ' << prefix_ << ' ';
}
