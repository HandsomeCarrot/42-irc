/**
 * \file Message.cpp
 * \brief the 'Message' class implementation
 * \author vpoka
 * \date 2026-05-18
 *
 *
 * \defgroup MessageParsing MessageParsing
 * \ingroup Messages
 * \brief How raw IRC messages get parsed into the internal Message class.
 *
 * \par IRC Message Format (RFC 1459 2.3.1)
 * An IRC message has the following structure:
 *
 *     [@tags] [:source] <command> [<middle>...] [:trailing]
 *
 * - \b Tags (optional) - introduced by `@` - IRCv3 extension (opaque token).
 * - \b Source (optional) - introduced by `:` - server name or `nick!user@host`.
 * - \b Command: either alphabetic letters or exactly 3 digits (RFC 1459).
 * - \b Middle params: (optional for some commands) no spaces/`\\0`/`\\r`/`\\n` inside.
 * - \b Trailing param (optional) - introduced by `:` - consumes the rest of the
 *   line. Spaces allowed, but `\\0`/`\\r`/`\\n` still forbidden.
 *
 * \par Internal Storage
 * A message is decomposed into five members:
 * - `_tags`                   - tag string without the leading `@`.
 * - `_source`                 - source string without the leading `:`.
 * - `_command`                - alphabetic commands are uppercased in-place; numerics
 *                               are left as-is.
 * - `_parameters`             - `std::vector<std::string>` of extracted parameters.
 * - `_has_trailing_parameter` - flag tracking whether the last parameter
 *                               is a trailing parameter (introduced by `:`).
 *
 * The message string is passed by value to parseMessage(), CR-LF is
 * stripped from it, and the copy is parsed with index-based traversal
 * (`substr()` + cursor advancement).
 *
 * \par Parsing Pipeline
 * parseMessage() walks the cleaned copy with a cursor, calling extractors in
 * a fixed order:
 *
 * 1. \b Tags - if `@` is at the cursor, read until next space, advance.
 * 2. \b Source - if `:` is at the cursor, read until next space, advance.
 * 3. \b Command - read the next space-delimited token and validate it.
 *    Alphabetic commands are uppercased. On failure the entire parse is
 *    aborted (command is mandatory).
 * 4. \b Parameters - loop collecting space-delimited middle parameters until
 *    a `:` is encountered, which triggers a trailing parameter (rest of the
 *    string) and stops. Enforced maximum: Message::max_parameters.
 *
 * \par End-to-End Example
 *
 *     Raw:  @id=234AB :dan!d@localhost PRIVMSG #chan :Hey what's up!\\r\\n
 *     Tags:       "id=234AB"
 *     Source:     "dan!d@localhost"
 *     Command:    "PRIVMSG"
 *     Parameters: ["#chan", "Hey what's up!"]
 */

#include "Message.hpp"			// Message
#include "../util/Logger.hpp"	// Logger
#include <cctype>				// std::isdigit()
#include <stdexcept>			// std::runtime_error()

namespace
{

	/**
	 * \ingroup MessageParsing
	 * \brief Removes potential CR-LF string endings from `message`.
	 */
	void removeCRLF(std::string & message)
	{
		if (message.empty())
			return ;

		if (*(message.end() - 1) == '\n' || *(message.end() - 1) == '\r')
			message.erase(message.end() - 1);

		if (message.empty())
			return ;

		if (*(message.end() - 1) == '\n' || *(message.end() - 1) == '\r')
			message.erase(message.end() - 1);
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Advances `pos` past consecutive space characters.
	 *
	 * \param string - the string to scan
	 * \param pos    - position to start skipping from
	 *
	 * \return index of the first non-space character, or \c string.size()
	 *         if only spaces remain.
	 */
	std::string::size_type skipSpaces(const std::string & string, std::string::size_type pos)
	{
		if (pos >= string.size())
			return (string.size());

		std::string::size_type next_pos = string.find_first_not_of(' ', pos);
		if (next_pos == std::string::npos)
			return (string.size());
		return (next_pos);
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Finds the next occurrence of `delimiter` in `string`.
	 *
	 * \param string    - the string to scan
	 * \param pos       - position to start searching from
	 * \param delimiter - the character to find
	 *
	 * \return index of the delimiter, or \c string.size() if not found.
	 */
	std::string::size_type findTokenEnd(const std::string & string, std::string::size_type pos, char delimiter)
	{
		std::string::size_type space_pos = string.find_first_of(delimiter, pos);
		if (space_pos == std::string::npos)
			return (string.size());
		return (space_pos);
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Extracts the `@`-introduced tag token (IRCv3).
	 *
	 * If `message[index]` is `@`, reads until the next space and stores
	 * the token in `tags` without the leading `@`. Otherwise returns
	 * `index` unchanged.
	 *
	 * \param message - the raw message string
	 * \param tags    - receives the extracted tag (without leading `@`)
	 * \param index   - starting position to check
	 *
	 * \return position of the space after the tag, \c message.size() if the
	 *         tag extends to end-of-string, or `index` if no tag is present.
	 */
	std::string::size_type extractTags(const std::string & message, std::string & tags, std::string::size_type index)
	{
		if (index >= message.size())
			throw std::runtime_error("command: empty.");

		if (message[index] != '@')
			return (index);

		++index;
		std::string::size_type next_space = findTokenEnd(message, index, ' ');

		tags = message.substr(index, next_space - index);
		
		Logger::debug << "\ttags(" << index - 1 << "," << next_space << "): \"" << tags << "\"" << std::endl;
		
		return (next_space);
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Extracts the `:`-introduced source prefix.
	 *
	 * If `message[index]` is `:`, reads until the next space and stores
	 * the token in `source` without the leading `:`. Otherwise returns
	 * `index` unchanged.
	 *
	 * \param message - the raw message string
	 * \param source  - receives the extracted source (without leading `:`)
	 * \param index   - starting position to check
	 *
	 * \return position of the space after the source, \c message.size() if
	 *         it extends to end-of-string, or `index` if no source present.
	 */
	std::string::size_type extractSource(const std::string & message, std::string & source, std::string::size_type index)
	{
		if (index >= message.size())
			throw std::runtime_error("command: empty.");

		if (message[index] != ':')
			return (index);

		++index;
		std::string::size_type next_space = findTokenEnd(message, index, ' ');

		source = message.substr(index, next_space - index);
		
		Logger::debug << "\tsource(" << index - 1 << "," << next_space << "): \"" << source << "\"" << std::endl;
		
		return (next_space);
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Validates and normalizes a command string (RFC 1459: 1*letter / 3digit).
	 *
	 * Numeric commands must be exactly 3 digits. Alphabetic commands must
	 * consist entirely of letters and are uppercased in-place. Mixing
	 * letters and digits is rejected.
	 *
	 * \param command - the string to validate and normalize
	 *
	 * \throw std::runtime_error - if `command` is empty.
	 * \throw std::runtime_error - if a numeric command is not exactly three digits.
	 * \throw std::runtime_error - if an alphabetic command contains non-letter characters.
	 */
	void parseCommand(std::string & command)
	{
		if (command.empty())
			throw std::runtime_error("command: empty.");
		else if (std::isdigit(command[0]))
		{
			if (command.size() != 3 || !std::isdigit(command[1]) || !std::isdigit(command[2]))
				throw std::runtime_error("command: numeric: syntax error.");
		}
		else
		{
			for (std::string::size_type i = 0; i < command.size(); ++i)
			{
				if (!std::isalpha(command[i]))
					throw std::runtime_error("command: alphabetic: syntax error.");
				command[i] = std::toupper(command[i]);
			}
		}
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Reads the command token and validates it via parseCommand().
	 *
	 * \param message - the raw message string
	 * \param command - receives the extracted command (uppercased if alphabetic)
	 * \param index   - starting position
	 *
	 * \return position after the command, or \c std::string::npos if invalid/out-of-bounds.
	 *
	 * \throw std::runtime_error - if `index` is past the end of `message` (no command present).
	 * \throw std::runtime_error - rethrows any exception from parseCommand().
	 */
	std::string::size_type extractCommand(const std::string & message, std::string & command, std::string::size_type index)
	{
		if (index >= message.size())
			throw std::runtime_error("command: empty.");

		std::string::size_type next_space = findTokenEnd(message, index, ' ');
		command = message.substr(index, next_space - index);

		parseCommand(command);

		Logger::debug << "\tcommand(" << index << "," << next_space << "): '" << command << "'" << std::endl;

		return (next_space);
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Validates a single parameter string (RFC 1459).
	 *
	 * Forbidden characters: \0, \r, \n (always); space (\c trailing==false only).
	 * A colon (\c :) as the first character of a middle parameter is also forbidden.
	 * Empty parameters are rejected.
	 *
	 * \param parameter - the string to validate
	 * \param trailing  - if \c true the parameter is a trailing parameter
	 *                    (spaces allowed, colon as first character allowed)
	 *
	 * \throw std::runtime_error - if `trailing == true` and `parameter` contains
	 *                             any of \\0, \\r, or \\n.
	 * \throw std::runtime_error - if `trailing == false` and `parameter` contains
	 *                             ':' as the first character, or any of ' ', \\0,
	 *                             \\r, or \\n.
	 * \note Loggs a debug message, if the given parameter is empty.
	 */
	void parseParameter(const std::string & parameter, bool trailing)
	{
		if (parameter.empty())
		{
			if (!trailing)
				throw std::runtime_error("parameter: middle: empty");
			return ;
		}

		std::string forbidden_chars = "\0\r\n";
		if (!trailing)
			forbidden_chars += " ";

		if ((!trailing && parameter[0] == ':')
			|| parameter.find_first_of(forbidden_chars) != std::string::npos)
		{
			if (trailing)
				throw std::runtime_error("parameter: trailing: syntax error.");
			throw std::runtime_error("parameter: middle: syntax error.");
		}
	}

	/**
	 * \ingroup MessageParsing
	 * \brief Extracts parameters from `message` starting at `index`.
	 *
	 * Collects space-delimited middle parameters. A `:` triggers a trailing
	 * parameter (rest of the string) and stops. Enforces Message::max_parameters (15).
	 * Each extracted parameter is validated by parseParameter().
	 *
	 * \param message           - the raw message string
	 * \param index             - starting position
	 * \param trailing_parameter - set to \c true if the last parameter is a trailing one
	 *
	 * \return vector of validated parameter strings (never empty on success).
	 *
	 * \note Logs warning message if the number of parameters exceeds Message::max_parameters.
	 *
	 * \throw std::runtime_error - rethrown from parseParameter() if any
	 *                             parameter fails validation (empty, forbidden
	 *                             characters, or ':' as first character of a
	 *                             middle parameter).
	 */
	Message::t_params extractParameters(const std::string & message, std::string::size_type index, bool & trailing_parameter)
	{
		std::string::size_type	next_space;
		Message::t_params		parameters;
	
		while (index < message.size())
		{
			if (parameters.size() >= Message::max_parameters)
			{
				Logger::info << "message: more than " << Message::max_parameters << " parameters found: overflow will be ignored." << std::endl;
				break ;
			}

			std::string current_param;

			if (message[index] == ':')
			{
				current_param = message.substr(index + 1);
				parseParameter(current_param, true);

				parameters.push_back(current_param);
				trailing_parameter = true;

				break ;
			}

			next_space = findTokenEnd(message, index, ' ');

			current_param = message.substr(index, next_space - index);
			parseParameter(current_param, false);

			parameters.push_back(current_param);
			index = skipSpaces(message, next_space);
		}

		Logger::debug << "\tparameters(" << index << "," << message.size() << "):";
		for (Message::t_params::size_type i = 0; i < parameters.size(); ++i)
			Logger::debug << " \"" << parameters[i] << "\"";
		Logger::debug << std::endl;

		return (parameters);
	}

} /** namespace */

/**
 * \ingroup Messages
 * \brief Maximum number of parameters a single command can have.
 *
 * Changes the behaviour of the IRC-message parser.
 *
 * \note IRC standard = 15
 */
const unsigned short Message::max_parameters = 15;

/**
 * \ingroup Messages
 * \brief IRC message line terminator (RFC 1459 2.3.1)
 *
 * Every IRC message must end with the two-character sequence
 * `\r\n` (0x0D 0x0A, carriage-return + line-feed).
 */
const std::string Message::terminator = "\r\n";

/**
 * \brief default constructor
 *
 * Everything is default initialized.
 */
Message::Message(void) :
	_sender_id(),
	_tags(),
	_source(),
	_command(),
	_parameters(),
	_has_trailing_parameter()
{
	//nothing happens
}

/**
 * \ingroup Messages
 * \ingroup MessageParsing
 * \brief Constructs from a raw IRC message string and sender id.
 *
 * Parses the raw message string into the internal components
 * (`_tags`, `_source`, `_command`, `_parameters`).
 *
 * \param message   - the raw wire-format message
 * \param sender_id - socket fd of the sending client;
 *                    `-1` (default) means the message originated from
 *                    this server
 *
 * \throw std::runtime_error - if parsing fails.
 */
Message::Message(const std::string & message, int sender_id) :
	_sender_id(sender_id),
	_tags(),
	_source(),
	_command(),
	_parameters(),
	_has_trailing_parameter(false)
{
	try
	{
		parseMessage(message);
	}
	catch (const std::exception & e)
	{
		throw std::runtime_error(std::string("invalid IRC message: ") + e.what());
	}
}

/**
 * \brief copy constructor
 *
 * Creates a deep copy of `other`.
 *
 * \param other - object to copy data from
 */
Message::Message(const Message & other) :
	_sender_id(other._sender_id),
	_tags(other._tags),
	_source(other._source),
	_command(other._command),
	_parameters(other._parameters),
	_has_trailing_parameter(other._has_trailing_parameter)
{
	//nothing happens
}

/**
 * \brief deconstructor
 *
 * Does nothing
 */
Message::~Message(void)
{
	//nothing happens
}

/**
 * \brief assignment operator
 *
 * Creates a deep copy of `other`.
 *
 * \param other - object to copy data from - (right)
 *
 * \return reference to the modified object - (left)
 */
Message & Message::operator=(const Message & other)
{
	if (this == &other)
		return (*this);

	_sender_id = other._sender_id;
	_tags = other._tags;
	_source = other._source;
	_command = other._command;
	_parameters = other._parameters;
	_has_trailing_parameter = other._has_trailing_parameter;

	return (*this);
}

/**
 * \ingroup Messages
 * \brief '_sender_id' getter
 *
 * \return the client id of the sender
 */
int Message::getSenderId(void) const
{
	return (_sender_id);
}

/**
 * \ingroup Messages
 * \brief `_tags` getter
 *
 * \return non-modifiable reference to objects `_tags` variable.
 */
const std::string &	Message::getTags(void) const
{
	return (_tags);
}

/**
 * \ingroup Messages
 * \brief `_source` getter
 *
 * \return non-modifiable reference to objects `_source` variable.
 */
const std::string &	Message::getSource(void) const
{
	return (_source);
}

/**
 * \ingroup Messages
 * \brief `_command` getter
 *
 * \return non-modifiable reference to objects `_command` variable.
 */
const std::string &	Message::getCommand(void) const
{
	return (_command);
}

/**
 * \ingroup Messages
 * \brief `_params` getter
 *
 * \return non-modifiable reference to objects `_params` variable.
 */
const Message::t_params & Message::getParams(void) const
{
	return (_parameters);
}

/**
 * \ingroup Messages
 * \brief `_has_trailing_parameter` getter
 *
 * Indicates whether the last parameter in `_parameters` was
 * introduced by `:` (a trailing parameter, RFC 1459 2.3.1).
 * Trailing parameters may contain spaces; middle parameters may not.
 *
 * \retval true  - last parameter is a trailing parameter
 * \retval false - last parameter is a middle parameter, or there
 *                 are no parameters at all
 */
bool Message::hasTrailingParameter(void) const
{
	return (_has_trailing_parameter);
}

/**
 * \ingroup Messages
 * \brief Reconstructs the IRC wire-format string from parsed components.
 *
 * Builds a message string in RFC 1459 format by reassembling
 * `_tags`, `_source`, `_command`, and `_parameters`. A trailing
 * parameter (if present) is prefixed with `:` and CR-LF is
 * appended.
 *
 * \note A message cannot be reconstructed without a valid command.
 *       If `_command` is empty, an empty string is returned and a
 *       warning is logged.
 *
 * \return reconstructed IRC message string suitable for sending
 *         over the wire, or an empty string on failure.
 */
const std::string Message::toString(void) const
{
	std::string message_string = "";

	if (!_tags.empty())
		message_string += "@" + _tags + " ";
	if (!_source.empty())
		message_string += ":" + _source + " ";

	message_string += _command;

	if (!_parameters.empty())
	{
		for (t_params::size_type i = 0; (i + 1) < _parameters.size(); ++i)
			message_string += " " + _parameters[i];

		message_string += " ";
		if (_has_trailing_parameter)
			message_string += ":";
		message_string += _parameters[_parameters.size() - 1];
	}

	message_string += terminator;

	return (message_string);
}

/**
 * \ingroup MessageParsing
 * \brief Parses a cleaned message copy into `_tags`, `_source`,
 *        `_command`, and `_parameters`.
 *
 * Walks the string with a cursor: tags → source → command → parameters.
 * If the command is invalid the entire parse is aborted.
 *
 * \param message - the cleaned message string (CR-LF already removed)
 *
 * \throw std::runtime_error - if `message` is empty.
 * \throw std::runtime_error - rethrows any exception from extractCommand()
 *                             or extractParameters().
 */
void Message::parseMessage(std::string message)
{
	Logger::info << "received message: " << message << std::endl;

	removeCRLF(message);

	if (message.empty())
		throw std::runtime_error("empty.");

	std::string::size_type index = 0;
	std::string::size_type new_index = 0;

	new_index = extractTags(message, _tags, index);

	if (index != new_index)
		index = skipSpaces(message, new_index);

	new_index = extractSource(message, _source, index);

	if (index != new_index)
		index = skipSpaces(message, new_index);

	new_index = extractCommand(message, _command, index);

	index = skipSpaces(message, new_index);

	_parameters = extractParameters(message, index, _has_trailing_parameter);
}
