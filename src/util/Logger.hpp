#ifndef LOGGER_HPP
# define LOGGER_HPP

# include "LogType.hpp"

class Logger
{
	public:
		enum e_level
		{
			LOGLVL_ERROR,
			LOGLVL_WARN,
			LOGLVL_INFO,
			LOGLVL_DEBUG,
			LOGLVL_VERBOSE
		};

		static void	setLogLevel(e_level level);
		static bool	isEnabled(e_level level);

		static LogType error;
		static LogType warn;
		static LogType info;
		static LogType debug;
		static LogType verbose;

		Logger(void);
		~Logger(void);
	private:
		Logger(const Logger & other);
		Logger & operator=(const Logger & other);

		static e_level	log_level_;
};

#endif /* LOGGER_HPP */
