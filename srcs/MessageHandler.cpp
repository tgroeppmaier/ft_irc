#include "MessageHandler.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <sys/epoll.h>
#include <arpa/inet.h> // For inet_ntoa


using std::vector;
using std::string;
using std::deque;
using std::map;

MessageHandler::MessageHandler(IrcServ& server) : server_(server) {
  command_map_["CAP"] = &MessageHandler::command_CAP;
  command_map_["NICK"] = &MessageHandler::command_NICK;
  command_map_["USER"] = &MessageHandler::command_USER;
  command_map_["PING"] = &MessageHandler::command_PING;
  command_map_["QUIT"] = &MessageHandler::command_QUIT;
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


void MessageHandler::send_message(Client& client, std::string& message) {
  size_t length = message.length();
  ssize_t bytes_sent = send(client.fd_, message.c_str(), length, MSG_NOSIGNAL);
  if (bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    // Modify the existing event to include EPOLLOUT
    epoll_event ev;
    ev.events = EPOLLOUT; // Listen for output events to see when socket is available again
    ev.data.fd = client.fd_;
    if (epoll_ctl(server_.ep_fd_, EPOLL_CTL_MOD, client.fd_, &ev) == -1) {
      perror("Error modifying client socket to include EPOLLOUT");
      return;
    }
    client.unsent_messages_.push_back(message);
  } 
  else if (bytes_sent == -1) {
    perror("Error sending message to client");
    server_.close_client_fd(client.fd_);
  }
}

void MessageHandler::process_incoming_messages(Client& client) {
  client.split_buffer();
  for (std::deque<std::string>::iterator it = client.received_messages_.begin(); it != client.received_messages_.end(); ++it) {
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
      string message  = "421 " + client.nick_ + " " + command + " :Unknown command\r\n";
      send_message(client, message);
      std::cout << "###Unknown command: " << command << std::endl;
    }
    client.received_messages_.pop_front();
  }
}



void MessageHandler::reply_ERR_NEEDMOREPARAMS(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    string message  = "461 " + client.nick_ + " " + arguments[0] + " :Not enough parameters\r\n";
    send_message(client, message);
  }
}

void MessageHandler::command_CAP(Client& client, std::vector<std::string>& arguments) {
  // std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
}

void MessageHandler::command_NICK(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    client.nick_ = arguments[0];
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
  }
  // std::cout << "Handling NICK command for client " << client.nick_<< std::endl;
}

void MessageHandler::command_PING(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    string message = "PONG \r\n";
    send_message(client, message);
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
  }
}

void MessageHandler::command_QUIT(Client& client, std::vector<std::string>& arguments) {
  // server_.close_client_fd(client.fd_);
}





void MessageHandler::command_USER(Client& client, std::vector<std::string>& arguments) {
  // print_args(client, arguments);

  if (arguments.size() < 4) {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
    return;
  }

  client.username_ = arguments[0];
  client.hostname_ = arguments[1];
  client.servername_ = arguments[2];
  client.realname_ = arguments[3];

  std::string response = "001 " + client.nick_ + " :Welcome to the IRC server\r\n";
  send_message(client, response);

  response = "002 " + client.nick_ + " :Your host is " + std::string(inet_ntoa(server_.server_addr_.sin_addr)) + ", running version 1.0\r\n";
  send_message(client, response);

  response = "003 " + client.nick_ + " :This server was created today\r\n";
  send_message(client, response);

  response = "004 " + client.nick_ + " " + std::string(inet_ntoa(server_.server_addr_.sin_addr)) + " 1.0 o o\r\n";
  send_message(client, response);

  response = "005 " + client.nick_ + " :Some additional information\r\n";
  send_message(client, response);

  // std::cout << "Handling USER command for client " << client.fd_ << std::endl;
}