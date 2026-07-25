#ifndef LOGTYPE_HPP
# define LOGTYPE_HPP

# include <string>
# include <iostream>

class LogType
{
	public:
		LogType(bool enabled, const std::string & prefix);
		~LogType(void);

		void	setEnabled(bool enable);

		template <typename T>
		LogType & operator<<(const T & value)
		{
			if (!enabled_)
				return (*this);

			writePrefix();
			target_stream_ << value;

			return (*this);
		}

		LogType & operator<<(std::ostream & (*function)(std::ostream &));

	private:
		LogType(void);
		LogType(const LogType & other);
		LogType & operator=(const LogType & other);

		void	writePrefix(void);

		bool		enabled_;
		bool		line_start_;
		std::string	prefix_;

		static std::ostream & target_stream_;
};

#endif /* LOGTYPE_HPP */
