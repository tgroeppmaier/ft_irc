#ifndef MESSAGEHANDLER_HPP
#define MESSAGEHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include "Client.hpp"
#include "IrcServ.hpp"


class IrcServ;

class MessageHandler {
private:
  IrcServ& server_;
  void process_messages(Client& client, const std::string& message);

public:
  MessageHandler(IrcServ& server);
  ~MessageHandler();

  void process_incoming_messages(Client& client);

  void send_messages(Client& client);
  // Command handling functions
  void command_CAP(Client& client, std::vector<std::string>& arguments);
  void command_NICK(Client& client, std::vector<std::string>& arguments);
  void command_USER(Client& client, std::vector<std::string>& arguments);
  void command_PING(Client& client, std::vector<std::string>& arguments);
  void command_QUIT(Client& client, std::vector<std::string>& arguments);
  void command_JOIN(Client& client, std::vector<std::string>& arguments);
  void reply_ERR_NEEDMOREPARAMS(Client& client, std::vector<std::string>& arguments);

  // Map of command strings to function pointers
  std::map<std::string, void(MessageHandler::*)(Client&, std::vector<std::string>&)> command_map_;


};

#endif