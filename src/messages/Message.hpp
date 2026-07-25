/**
 * \file Message.hpp
 * \brief the `Message` class definition
 * \author vpoka
 * \date 2026-05-18
 *
 *
 * \defgroup Messages Messages
 * \brief Internal representation of a parsed IRC message.
 *
 * The `Message` class is the server's internal representation of a single
 * IRC message received from a client or another server. A raw wire-format
 * string (e.g. `"PRIVMSG #chan :Hello\r\n"`) is decomposed into its
 * semantic components - tags, source, command, and parameters - according
 * to RFC 1459 2.3.1 with IRCv3 tag support.
 *
 * How a raw message is converted to the internal class
 * is defined in the \ref MessageParsing group.
 *
 * \relates MessageParsing - internal parsing pipeline documentation.
 */

#ifndef MESSAGE_HPP
 #define MESSAGE_HPP

#include <string>
#include <vector>

/**
 * \ingroup Messages
 *
 * \brief Internal representation of an IRC message.
 *
 * A raw message split into its components — tags, source, command, and
 * parameters.
 *
 * `_sender_id` holds the socket file descriptor of the client that sent
 * the message, or `-1` when the message originated from this server.
 *
 * `_has_trailing_parameter` tracks whether the last parameter is a
 * trailing parameter. See \ref hasTrailingParameter() for details.
 *
 * \see \ref MessageParsing for the parsing pipeline details.
 */
class Message
{
	public:
		typedef std::vector<std::string> t_params;

		static const unsigned short	max_parameters;
		static const std::string	terminator;

	private:
		int			_sender_id;

		std::string	_tags;
		std::string	_source;
		std::string	_command;
		t_params	_parameters;

		bool		_has_trailing_parameter;

		Message(void);

	public:
		explicit Message(const std::string & message, int sender_id = -1);
		Message(const Message & other);
		~Message(void);

		Message	&	operator=(const Message & other);

		int					getSenderId(void) const;
		const std::string &	getTags(void) const;
		const std::string &	getSource(void) const;
		const std::string &	getCommand(void) const;
		const t_params &	getParams(void) const;
		bool				hasTrailingParameter(void) const;

		const std::string	toString(void) const;

	private:
		void	parseMessage(std::string message);
};

#endif /* MESSAGE_HPP */