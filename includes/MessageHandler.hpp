#ifndef MESSAGEHANDLER_HPP
#define MESSAGEHANDLER_HPP

#include <string>
#include <vector>
#include "Client.hpp"
#include "IrcServ.hpp"

class IrcServ;

class MessageHandler {
public:
  MessageHandler(IrcServ& server);

  void process_incoming_messages(Client& client);

private:
  IrcServ& server_;
  void split_buffer(Client& client);

  // Get command from a message
  // void get_command(string& message);
};

#endif 