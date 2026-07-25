#ifndef REPLIES_HPP
# define REPLIES_HPP

#include <string>

inline std::string RPL_WELCOME(const std::string& nick, const std::string& user, const std::string& host)
{
	return (":" + host + " 001 " + nick + " :Welcome to the IRC Network " + nick + "!" + user + "@" + host);
}

inline std::string RPL_YOURHOST(const std::string& nick, const std::string& host)
{
	return (":" + host + " 002 " + nick + " :Your host is " + host + ", running version 1.0");
}

inline std::string RPL_CREATED(const std::string& nick, const std::string& host, const std::string& date)
{
	return (":" + host + " 003 " + nick + " :This server was created " + date);
}

inline std::string RPL_MYINFO(const std::string& nick, const std::string& host)
{
	return (":" + host + " 004 " + nick + " " + host + " 1.0 o itkol");
}

inline std::string RPL_NOTOPIC(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 331 " + client + " " + channel + " :No topic is set");
}

inline std::string RPL_TOPIC(const std::string& client, const std::string& host, const std::string& channel, const std::string& topic)
{
	return (":" + host + " 332 " + client + " " + channel + " :" + topic);
}

inline std::string RPL_INVITING(const std::string& client, const std::string& host, const std::string& nick, const std::string& channel)
{
	return (":" + host + " 341 " + client + " " + nick + " " + channel);
}

inline std::string RPL_NAMREPLY(const std::string& client, const std::string& host, const std::string& symbol, const std::string& channel, const std::string& users)
{
	return (":" + host + " 353 " + client + " " + symbol + " " + channel + " :" + users);
}

inline std::string RPL_ENDOFNAMES(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 366 " + client + " " + channel + " :End of /NAMES list.");
}

inline std::string RPL_ENDOFBANLIST(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 368 " + client + " " + channel + " :End of channel ban list");
}
inline std::string ERR_UNKNOWNMODE(const std::string& client, const std::string& host, const std::string& modechar)
{
	return (":" + host + " 472 " + client + " " + modechar + " :is unknown mode char to me");
}

inline std::string ERR_NOSUCHNICK(const std::string& client, const std::string& host, const std::string& nick)
{
	return (":" + host + " 401 " + client + " " + nick + " :No such nick/channel");
}

inline std::string ERR_NOSUCHCHANNEL(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 403 " + client + " " + channel + " :No such channel");
}

inline std::string ERR_NOSUCHCOMMAND(const std::string& client, const std::string& host, const std::string& command)
{
	return (":" + host + " 421 " + client + " " + command + " :Unknown command");
}

inline std::string ERR_NONICKNAMEGIVEN(const std::string& client, const std::string& host)
{
	return (":" + host + " 431 " + client + " :No nickname given");
}

inline std::string ERR_NICKNAMEINUSE(const std::string& client, const std::string& host, const std::string& nick)
{
	return (":" + host + " 433 " + client + " " + nick + " :Nickname is already in use");
}

inline std::string ERR_ERRONEUSNICKNAME(const std::string& client, const std::string& host, const std::string& nick)
{
	return (":" + host + " 432 " + client + " " + nick + " :Erroneous nickname");
}

inline std::string ERR_NORECIPIENT(const std::string& client, const std::string& host, const std::string& command)
{
	return (":" + host + " 411 " + client + " " + command + " :No recipient given");
}

inline std::string ERR_NOTEXTTOSEND(const std::string& client, const std::string& host)
{
	return (":" + host + " 412 " + client + " :No text to send");
}

inline std::string ERR_NOTREGISTERED(const std::string& client, const std::string& host)
{
	return (":" + host + " 451 " + client + " :You have not registered");
}

inline std::string ERR_NOTONCHANNEL(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 442 " + client + " " + channel + " :You're not on that channel");
}

inline std::string ERR_USERONCHANNEL(const std::string& client, const std::string& host, const std::string& nick, const std::string& channel)
{
	return (":" + host + " 443 " + client + " " + nick + " " + channel + " :is already on channel");
}

inline std::string ERR_USERNOTINCHANNEL(const std::string& client, const std::string& host, const std::string& nick, const std::string& channel)
{
	return (":" + host + " 441 " + client + " " + nick + " " + channel + " :They aren't on that channel");
}

inline std::string ERR_CHANOPRIVSNEEDED(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 482 " + client + " " + channel + " :You're not channel operator");
}

inline std::string ERR_UMODEUNKNOWNFLAG(const std::string& client, const std::string& host)
{
	return (":" + host + " 501 " + client + " :Unknown MODE flag");
}

inline std::string ERR_USERSDONTMATCH(const std::string& client, const std::string& host)
{
	return (":" + host + " 502 " + client + " :Cannot change mode for other users");
}

inline std::string ERR_NEEDMOREPARAMS(const std::string& client, const std::string& host, const std::string& command)
{
	return (":" + host + " 461 " + client + " " + command + " :Not enough parameters");
}

inline std::string ERR_ALREADYREGISTRED(const std::string& client, const std::string& host)
{
	return (":" + host + " 462 " + client + " :You may not reregister");
}

inline std::string ERR_PASSWDMISMATCH(const std::string& client, const std::string& host)
{
	return (":" + host + " 464 " + client + " :Password incorrect");
}

inline std::string ERR_CHANNELISFULL(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 471 " + client + " " + channel + " :Cannot join channel (+l)");
}

inline std::string ERR_INVITEONLYCHAN(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 473 " + client + " " + channel + " :Cannot join channel (+i)");
}
inline std::string ERR_BADCHANNELKEY(const std::string& client, const std::string& host, const std::string& channel)
{
	return (":" + host + " 475 " + client + " " + channel + " :Cannot join channel (+k)");
}

inline std::string RPL_CHANNELMODEIS(const std::string& client, const std::string& host, const std::string& channel, const std::string& modes, const std::string& params)
{
	std::string r = ":" + host + " 324 " + client + " " + channel + " " + modes;
	if (!params.empty())
		r += " " + params;
	return (r);
}

inline std::string RPL_UMODEIS(const std::string& client, const std::string& host, const std::string& modes)
{
	return (":" + host + " 221 " + client + " " + modes);
}

#endif
