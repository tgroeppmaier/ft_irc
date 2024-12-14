#ifndef MESSAGEHANDLER_HPP
#define MESSAGEHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include "Client.hpp"
#include "IrcServ.hpp"
#include <sstream>

#define REPLY_ERR_NOSUCHNICK(client, nick) \
  client.add_message_out("401 " + client.nick_ + " " + nick + " :No such nick/channel\r\n")

#define REPLY_ERR_NOSUCHCHANNEL(client, channel_name) \
  client.add_message_out("403 " + client.nick_ + " " + channel_name + " :Invalid channel name\r\n")

#define REPLY_ERR_TOOMANYCHANNELS(client, channel_name) \
  client.add_message_out("405 " + client.nick_ + " " + channel_name + " :You have joined too many channels\r\n")

#define REPLY_ERR_NORECIPIENT(client, command) \
  client.add_message_out("411 " + client.nick_ + " :No recipient given (" + command + ")\r\n")

#define REPLY_ERR_NOTEXTTOSEND(client) \
  client.add_message_out("412 " + client.nick_ + " :No text to send\r\n")

#define REPLY_ERR_ERRONEUSNICKNAME(client, nick) \
  client.add_message_out("432 " + client.nick_ + " " + nick + " :Erroneous nickname\r\n")

#define REPLY_ERR_USERNOTINCHANNEL(client, channel) \
  client.add_message_out("441 " + client.nick_ + " " + channel + " :They aren't on that channel\r\n")

#define REPLY_ERR_NOTONCHANNEL(client, channel_name) \
  client.add_message_out("442 " + client.nick_ + " " + channel_name + " :You're not on that channel\r\n")

#define REPLY_ERR_USERONCHANNEL(client, nick, channel_name) \
  client.add_message_out("443 " + client.nick_ + " " + nick + " " + channel_name + " :is already on channel\r\n")

#define REPLY_ERR_NEEDMOREPARAMS(client, command) \
  client.add_message_out("461 " + client.nick_ + " " + command + " :Not enough parameters\r\n")

#define REPLY_ERR_CHANNELISFULL(client, channel_name) \
  client.add_message_out("471 " + client.nick_ + " " + channel_name + " :Cannot join channel (+l) - channel is full\r\n")

#define REPLY_ERR_INVITEONLYCHAN(client, channel_name) \
  client.add_message_out("473 " + client.nick_ + " " + channel_name + " :Cannot join channel (+i)\r\n")

#define REPLY_ERR_BADCHANNELKEY(client, channel_name) \
  client.add_message_out("475 " + client.nick_ + " " + channel_name + " :Cannot join channel (+k) - bad key\r\n")

#define REPLY_ERR_CHANOPRIVSNEEDED(client, channel_name) \
  client.add_message_out("482 " + client.nick_ + " " + channel_name + " :You're not channel operator\r\n")

class IrcServ;
class Channel;
class Client;

class MessageHandler {
private:
  IrcServ& server_;
  // Map of command strings to function pointers
  std::map<std::string, void(MessageHandler::*)(Client&, std::stringstream&)> command_map_;

  void ERR_NOTREGISTERED(Client& client);
  std::string create_message(Client& client, const std::string& command, const std::string& parameters, const std::string& message);
  std::string extract_message(std::stringstream& message);
  bool client_registered(Client& client);
  
  // Command handling functions
  void command_CAP(Client& client, std::stringstream& message);
  void command_NICK(Client& client, std::stringstream& message);
  void command_USER(Client& client, std::stringstream& message);
  void command_PING(Client& client, std::stringstream& message);
  void command_QUIT(Client& client, std::stringstream& message);
  void command_JOIN(Client& client, std::stringstream& message);
  void command_MODE(Client& client, std::stringstream& message);
  void command_PRIVMSG(Client& client, std::stringstream& message);
  void command_PASS(Client& client, std::stringstream& message);
  void command_KICK(Client& client, std::stringstream& message);
  void command_INVITE(Client& client, std::stringstream& message);
  void command_TOPIC(Client& client, std::stringstream& message);
  void command_PART(Client& client, std::stringstream& message);

  void reply_TOPIC(Client& client, Channel& channel);


public:
  MessageHandler(IrcServ& server);
  ~MessageHandler();

  void process_incoming_messages(Client& client);
  void send_messages(Client& client);

};

#endif