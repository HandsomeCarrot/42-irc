/**
 * @file IrcCase.hpp
 * @brief RFC 1459 casemapping helpers
 */
#ifndef IRC_CASE_HPP
# define IRC_CASE_HPP

# include <string>

/**
 * RFC 1459 casemap: A-Z, [\]~ fold to a-z, {|}`.
 */
inline std::string ircToLower(const std::string &s)
{
	std::string result = s;
	for (std::string::size_type i = 0; i < result.size(); ++i)
	{
		char c = result[i];
		if (c >= 'A' && c <= 'Z')
			result[i] = static_cast<char>(c - 'A' + 'a');
		else if (c == '[')
			result[i] = '{';
		else if (c == ']')
			result[i] = '}';
		else if (c == '\\')
			result[i] = '|';
		else if (c == '~')
			result[i] = '`';
	}
	return (result);
}

#endif
