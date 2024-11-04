#include "MessageHandler.hpp"
#include <iostream>
#include <sstream>


using std::vector;
using std::string;
using std::deque;
using std::map;

MessageHandler::MessageHandler(IrcServ& server) : server_(server) {
  command_map_["CAP"] = &MessageHandler::command_CAP;
  command_map_["NICK"] = &MessageHandler::command_NICK;
  command_map_["USER"] = &MessageHandler::command_USER;
}

MessageHandler::~MessageHandler() {
  command_map_.clear();
}

// void print_args(Client& client, vector<string>& arguments) {
//   for(vector<string>::iterator it = arguments.begin(); it != arguments.end(); ++it) {
//     cout << *it << ' ';
//   }
//   cout << endl;
// }

// void MessageHandler::split_buffer(Client& client) {
//   if (client.buffer_msg_from_.empty()) {
//     return;
//   }
//   size_t start = 0;
//   size_t end;
//   while ((end = client.buffer_msg_from_.find("\r\n", start)) != std::string::npos) {
//     client.messages_.push_back(client.buffer_msg_from_.substr(start, end - start));
//     start = end + 2;
//   }
//   if (start > client.buffer_msg_from_.size()) {
//     start -= 2;
//   }
//   client.buffer_msg_from_.erase(0, start);
// }

void MessageHandler::send_message(Client& client, const char* message, size_t length) {
  ssize_t bytes_sent = send(client.fd_, message, length, 0);
  if ((bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
    UnsentMessage full_message = {client.fd_, length, message};
    unsent_messages_.push_back(full_message);
  }
    // cout << "unsent: " << unsent_messages_.back().message << endl;
}

void MessageHandler::process_incoming_messages(Client& client) {
  client.split_buffer();
  for (std::vector<std::string>::iterator it = client.messages_.begin(); it != client.messages_.end(); ++it) {
    std::stringstream ss(*it);
    std::string command;
    std::vector<std::string> arguments;
    if (ss >> command) {
      std::string arg;
      while (ss >> arg) {
          arguments.push_back(arg);
      }
    } else {
        continue;
    }
    std::map<std::string, void(MessageHandler::*)(Client&, std::vector<std::string>&)>::iterator cmd_it = command_map_.find(command);
    if (cmd_it != command_map_.end()) {
        (this->*(cmd_it->second))(client, arguments);
    } else {
      string message  = "421 " + client.nick_ + " " + command + " :Unknown command";
      // send_message(client, message);
        std::cout << "***Unknown command:*** " << command << std::endl;
    }
  }
  client.messages_.clear();
}



void MessageHandler::command_CAP(Client& client, std::vector<std::string>& arguments) {
  // std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
}

void MessageHandler::command_NICK(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    client.nick_ = arguments[0];
  }
  // std::cout << "Handling NICK command for client " << client.nick_<< std::endl;
}

void MessageHandler::command_USER(Client& client, std::vector<std::string>& arguments) {
  // print_args(client, arguments);

  if (arguments.size() < 4) {
    std::string response = "461 " + client.nick_ + " USER :Not enough parameters\r\n";
    send_message(client, response.c_str(), response.length());
    return;
  }

  client.username_ = arguments[0];
  client.hostname_ = arguments[1];
  client.servername_ = arguments[2];
  client.realname_ = arguments[3];

  string response = "001 " + client.nick_ + " :Welcome to the IRC server\r\n";
  send_message(client, response.c_str(), response.length());

  response = "002 " + client.nick_ + " :Your host is " + "server_addr_.sin_addr.s_addr "+ ", running version 1.0\r\n";
  send_message(client, response.c_str(), response.length());

  response = "003 " + client.nick_ + " :This server was created today\r\n";
  send_message(client, response.c_str(), response.length());

  response = "004 " + client.nick_ + " " + "server_addr_.sin_addr.s_addr" + " 1.0 o o\r\n";
  send_message(client, response.c_str(), response.length());
  std::cout << "Handling USER command for client " << client.fd_ << std::endl;
}