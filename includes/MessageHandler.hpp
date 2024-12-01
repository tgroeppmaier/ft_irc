#ifndef MESSAGEHANDLER_HPP
#define MESSAGEHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include "Client.hpp"
#include "IrcServ.hpp"
#include <sstream>

#define REPLY_ERR_NEEDMOREPARAMS(client, command) \
  client.add_message_out("461 " + client.nick_ + " " + command + " :Not enough parameters\r\n")

#define REPLY_ERR_USERNOTINCHANNEL(client, channel) \
  client.add_message_out("441 " + client.nick_ + " " + channel + " :They aren't on that channel\r\n")

#define REPLY_ERR_NOSUCHCHANNEL(client, channel_name) \
  client.add_message_out("403 " + client.nick_ + " " + channel_name + " :Invalid channel name\r\n")

#define REPLY_ERR_CHANOPRIVSNEEDED(client, channel_name) \
  client.add_message_out("482 " + client.nick_ + " " + channel_name + " :You're not channel operator\r\n")

#define REPLY_ERR_NOTONCHANNEL(client, channel_name) \
  client.add_message_out("442 " + client.nick_ + " " + channel_name + " :You're not on that channel\r\n")

#define REPLY_ERR_TOOMANYCHANNELS(client, channel_name) \
  client.add_message_out("405 " + client.nick_ + " " + channel_name + " :You have joined too many channels\r\n")



class IrcServ;
class Channel;
class Client;

class MessageHandler {
private:
  IrcServ& server_;
  // Map of command strings to function pointers
  std::map<std::string, void(MessageHandler::*)(Client&, std::stringstream&)> command_map_;

  void ERR_NOTREGISTERED(Client& client);
  void handle_channel_mode(Client& client, const std::string& channel_name, std::stringstream& message);
  std::string create_message(Client& client, const std::string& command, const std::string& parameters, const std::string& message);
  std::string extract_message(std::stringstream& message);
  bool client_registered(Client& client);
  
  
  // void handle_user_mode(Client& client, const std::string& target_nick, std::vector<std::string>& arguments);

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


public:
  MessageHandler(IrcServ& server);
  ~MessageHandler();

  void process_incoming_messages(Client& client);
  void send_messages(Client& client);

};

#endif