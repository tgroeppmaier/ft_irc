#include "MessageHandler.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <sys/epoll.h>
#include <string>
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
  command_map_["JOIN"] = &MessageHandler::command_JOIN;
  command_map_["MODE"] = &MessageHandler::command_MODE;
  command_map_["PRIVMSG"] = &MessageHandler::command_PRIVMSG;
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


void MessageHandler::send_messages(Client& client) {
  size_t length = client.messages_outgoing_.length();
  ssize_t bytes_sent = send(client.fd_, client.messages_outgoing_.c_str(), length, MSG_NOSIGNAL);
  // if (bytes_sent > 0) {
  //   cout << client.messages_outgoing_.substr(0, static_cast<size_t>(bytes_sent)) << std::endl; // DEBUG
  // }
  if (bytes_sent == static_cast<ssize_t>(length)) { // all messages have been sent
    client.messages_outgoing_.clear();
    epoll_event ev;
    ev.events = EPOLLIN; // Listen for input events only
    ev.data.fd = client.fd_;
    if (epoll_ctl(server_.ep_fd_, EPOLL_CTL_MOD, client.fd_, &ev) == -1) {
      perror("Error modifying client socket to include EPOLLOUT");
    }
  }
  else if (bytes_sent > 0) { // partial send, try again next loop
    client.messages_outgoing_.erase(0, static_cast<size_t>(bytes_sent));
  }
  else if (bytes_sent == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
    perror("Error sending message to client");
    server_.delete_client(client.fd_);
  }
}

// void MessageHandler::process_messages(Client& client, const string& message) {

// }

void MessageHandler::process_incoming_messages(Client& client) {
  size_t start = 0;
  size_t end = 0;
  while ((end = client.messages_incoming_.find("\r\n", start)) != string::npos) {
    string message = client.messages_incoming_.substr(start, end - start);
    // cout << message << std::endl; // DEBUG
    start = end + 2;
    std::stringstream ss(message);
    vector<string> arg;
    string tmp, command;
    if (!(ss >> command)) {
      std::cerr << "Error extracting command" << std::endl;
      return;
    }
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
      client.messages_outgoing_.append(message);
      std::cout << "Unknown command: " << command << std::endl;
    }
  }
  client.messages_incoming_.erase(0, start);
  server_.epoll_in_out(client.fd_);
  // cout << client.messages_outgoing_ << std::endl; // DEBUG
}

void MessageHandler::reply_ERR_NEEDMOREPARAMS(Client& client, std::vector<std::string>& arguments) {
  if (!arguments[0].empty()) {
    string message  = "461 " + client.nick_ + " " + arguments[0] + " :Not enough parameters\r\n";
    client.messages_outgoing_.append(message);
    server_.epoll_in_out(client.fd_);
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
    client.messages_outgoing_.append(message);
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
  }
}

void MessageHandler::command_QUIT(Client& client, std::vector<std::string>& arguments) {
  cout << "closing fd: " << client.fd_ << std::endl;
  server_.delete_client(client.fd_);
}

void MessageHandler::command_JOIN(Client& client, std::vector<std::string>& arg) {
  vector<string> tokens;

  for (vector<string>::const_iterator it = arg.begin(); it != arg.end(); ++it) {
    string token;
    std::stringstream ss(*it);
    while(std::getline(ss, token, ',')) {
      if (!token.empty()) {
        tokens.push_back(token);
      }
    }
  }
  vector<string> channels;
  vector<string> keys;
  for (vector<string>::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
    string tmp = *it;
    if (!tmp.empty() && (tmp[0] == '#' || tmp[0] == '&')) {
      channels.push_back(tmp);
    }
    else {
      keys.push_back(tmp);
    }
  }
  map<string, Channel*>::iterator it = server_.channels_.find(channels[0]);
  if (it == server_.channels_.end()) {
    server_.create_channel(channels[0], client);
  }
  else {
    server_.join_channel(channels[0], client);
  }
}

void MessageHandler::command_PRIVMSG(Client& sender, std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    reply_ERR_NEEDMOREPARAMS(sender, arguments);
    return;
  }

  std::string target = arguments[0];
  // Channel* channel;

  // Target is a channel
  if (target[0] == '#' || target[0] == '&') {
    Channel* channel = server_.get_channel(target);
    if (channel == NULL) {
      std::string message = "403 " + sender.nick_ + " " + target + " :No such channel\r\n";
      sender.messages_outgoing_.append(message);
      server_.epoll_in_out(sender.fd_);
      return;
    }
    std::ostringstream oss;
    oss << ":" << sender.nick_ << "!" << sender.username_ << " PRIVMSG " << channel->name_ << " ";
     // Append the message content from the arguments
    for (size_t i = 1; i < arguments.size(); ++i) {
      if (i > 1) {
        oss << " ";
      }
      oss << arguments[i];
    }
    oss << "\r\n";

    // Broadcast the message to the channel
    // std::string message = oss.str();
    channel->broadcast(sender.fd_, oss.str());
    cout << "Broadcast: " << oss.str() << std::endl;

    // Handle sending message to the channel
  } else {
    // Handle sending message to a user
  }
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
  client.messages_outgoing_.append(message);

  message = "002 " + client.nick_ + " :Your host is " + std::string(inet_ntoa(server_.server_addr_.sin_addr)) + ", running version 1.0\r\n";
  client.messages_outgoing_.append(message);

  message = "003 " + client.nick_ + " :This server was created today\r\n";
  client.messages_outgoing_.append(message);

  message = "004 " + client.nick_ + " " + std::string(inet_ntoa(server_.server_addr_.sin_addr)) + " 1.0 o o\r\n";
  client.messages_outgoing_.append(message);

  message = "005 " + client.nick_ + " :Some additional information\r\n";
  client.messages_outgoing_.append(message);

  std::cout << "Handling USER command for client " << client.username_ << std::endl;
}



void MessageHandler::command_MODE(Client& client, std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    reply_ERR_NEEDMOREPARAMS(client, arguments);
    return;
  }

  std::string target = arguments[0];

  // Check if the target is a channel or a user
  if (target[0] == '#' || target[0] == '&') {
    // Handle channel mode
    handle_channel_mode(client, target, arguments);
  } else {
    // // Handle user mode
    // handle_user_mode(client, target, arguments);
  }
}

void MessageHandler::handle_channel_mode(Client& client, const string& channel_name, vector<string>& arguments) {
  std::map<std::string, Channel*>::iterator it = server_.channels_.find(channel_name);
  if (it == server_.channels_.end()) {
    // Channel does not exist
    std::string message = "403 " + client.nick_ + " " + channel_name + " :No such channel\r\n";
    client.messages_outgoing_.append(message);
    return;
  }

  Channel* channel = it->second;

  if (arguments.size() == 1) {
    // No mode changes, just return the current mode
    std::string message = "324 " + client.nick_ + " " + channel_name + " \r\n"; // Simplified, should include actual modes
    client.messages_outgoing_.append(message);
    server_.epoll_in_out(client.fd_);
  } 
  // else {
  //   // Apply mode changes
  //   std::string mode_changes = arguments[1];
  //   std::string message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " MODE " + channel_name + " " + mode_changes + "\r\n";
  //   channel->broadcast(message);
  // }
}

// void MessageHandler::handle_user_mode(Client& client, const std::string& target_nick, std::vector<std::string>& arguments) {
//   std::map<int, Client*>::iterator it = server_.clients_.find(client.fd_);
//   if (it == server.clients_.end() || it->second->nick_ != target_nick) {
//     // User does not exist or target is not the client itself
//     std::string message = "502 " + client.nick_ + " :Cannot change mode for other users\r\n";
//     client.messages_outgoing_.append(message);
//     return;
//   }

//   if (arguments.size() == 1) {
//     // No mode changes, just return the current mode
//     std::string message = "221 " + client.nick_ + " +\r\n"; // Simplified, should include actual modes
//     client.messages_outgoing_.append(message);
//   } else {
//     // Apply mode changes
//     // This is a simplified example, actual mode handling would be more complex
//     std::string mode_changes = arguments[1];
//     std::string message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " MODE " + target_nick + " " + mode_changes + "\r\n";
//     client.messages_outgoing_.append(message);
//   }
// }
