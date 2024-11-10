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
using std::cout;

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

void MessageHandler::save_message(Client& client, std::string& message) {
  client.unsent_messages_.append(message);
}


void MessageHandler::send_messages(Client& client) {
  size_t length = client.unsent_messages_.length();
  ssize_t bytes_sent = send(client.fd_, client.unsent_messages_.c_str(), length, MSG_NOSIGNAL);
  cout << client.unsent_messages_ << std::endl;
  if (bytes_sent == static_cast<ssize_t>(length)) {
    client.unsent_messages_.clear();
    epoll_event ev;
    ev.events = EPOLLIN; // Listen for input events only
    ev.data.fd = client.fd_;
    if (epoll_ctl(server_.ep_fd_, EPOLL_CTL_MOD, client.fd_, &ev) == -1) {
      perror("Error modifying client socket to include EPOLLOUT");
    }
  }
  else if (bytes_sent > 0) {
    client.unsent_messages_.erase(0, static_cast<size_t>(bytes_sent));
    epoll_event ev;
    ev.events = EPOLLIN; // Listen for input events only
    ev.data.fd = client.fd_;
    if (epoll_ctl(server_.ep_fd_, EPOLL_CTL_MOD, client.fd_, &ev) == -1) {
      perror("Error modifying client socket to include EPOLLOUT");
    }
  }
  else if (bytes_sent == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
    perror("Error sending message to client");
    server_.close_client_fd(client.fd_);
  }
}

void MessageHandler::process_messages(Client& client, const string& message) {

}

void MessageHandler::process_incoming_messages(Client& client) {
  size_t start = 0;
  size_t end = 0;
  while ((end = client.buffer_in.find("\r\n", start)) != string::npos) {
    string message = client.buffer_in.substr(start, end - start);
    // cout << message << std::endl;
    start = end + 2;
    std::stringstream ss(message);
    vector<string> arg;
    string tmp;
    string command;
    ss >> command;
    while (ss >> tmp) {
      arg.push_back(tmp);
    }
    map<string, void(MessageHandler::*)(Client&, vector<string>&)>::iterator cmd_it = command_map_.find(command);
    if (cmd_it != command_map_.end()) {
        (this->*(cmd_it->second))(client, arg);
        if (command == "QUIT") 
          return;
    } else {
      message  = "421 " + client.nick_ + " " + command + " :Unknown command\r\n";
      client.unsent_messages_.append(message);
      std::cout << "###Unknown command: " << command << std::endl;
    }
  }
  client.buffer_in.erase(0, start);
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT; // Listen for both input and output events
  ev.data.fd = client.fd_;
  if (epoll_ctl(server_.ep_fd_, EPOLL_CTL_MOD, client.fd_, &ev) == -1) {
    perror("Error modifying client socket to include EPOLLOUT");
    return;
  }
  // cout << client.unsent_messages_ << std::endl; // DEBUG
}

void MessageHandler::reply_ERR_NEEDMOREPARAMS(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    string message  = "461 " + client.nick_ + " " + arguments[0] + " :Not enough parameters\r\n";
    client.unsent_messages_.append(message);
  }
}

void MessageHandler::command_CAP(Client& client, vector<string>& arguments) {
  std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
}

void MessageHandler::command_NICK(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    client.nick_ = arguments[0];
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
  }
  std::cout << "Handling NICK command for client " << client.nick_<< std::endl;
}

void MessageHandler::command_PING(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    string message = "PONG \r\n";
    client.unsent_messages_.append(message);
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
  }
}

void MessageHandler::command_QUIT(Client& client, std::vector<std::string>& arguments) {
  server_.close_client_fd(client.fd_);
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

  std::string message = "001 " + client.nick_ + " :Welcome to the IRC server\r\n";
  client.unsent_messages_.append(message);

  message = "002 " + client.nick_ + " :Your host is " + std::string(inet_ntoa(server_.server_addr_.sin_addr)) + ", running version 1.0\r\n";
  client.unsent_messages_.append(message);

  message = "003 " + client.nick_ + " :This server was created today\r\n";
  client.unsent_messages_.append(message);

  message = "004 " + client.nick_ + " " + std::string(inet_ntoa(server_.server_addr_.sin_addr)) + " 1.0 o o\r\n";
  client.unsent_messages_.append(message);

  message = "005 " + client.nick_ + " :Some additional information\r\n";
  client.unsent_messages_.append(message);

  std::cout << "Handling USER command for client " << client.username_ << std::endl;
}