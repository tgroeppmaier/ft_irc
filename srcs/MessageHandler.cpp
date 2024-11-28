#include "MessageHandler.hpp"
#include <iostream>
#include <cstdio>
#include <sys/epoll.h>
#include <string>
#include <arpa/inet.h> // For inet_ntoa


using std::vector;
using std::string;
using std::deque;
using std::map;
using std::cout;
using std::stringstream;

MessageHandler::MessageHandler(IrcServ& server) : server_(server) {
  command_map_["CAP"] = &MessageHandler::command_CAP;
  command_map_["NICK"] = &MessageHandler::command_NICK;
  command_map_["USER"] = &MessageHandler::command_USER;
  command_map_["PING"] = &MessageHandler::command_PING;
  command_map_["QUIT"] = &MessageHandler::command_QUIT;
  command_map_["JOIN"] = &MessageHandler::command_JOIN;
  command_map_["MODE"] = &MessageHandler::command_MODE;
  command_map_["PRIVMSG"] = &MessageHandler::command_PRIVMSG;
  command_map_["PASS"] = &MessageHandler::command_PASS;
  command_map_["KICK"] = &MessageHandler::command_KICK;
}

MessageHandler::~MessageHandler() {
  command_map_.clear();
}

void MessageHandler::send_messages(Client& client) {
  size_t length = client.messages_outgoing_.length();
  ssize_t bytes_sent = send(client.fd_, client.messages_outgoing_.c_str(), length, MSG_NOSIGNAL);
  if (bytes_sent > 0) {
    cout << client.messages_outgoing_.substr(0, static_cast<size_t>(bytes_sent)) << std::endl; // DEBUG
  }
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
    string command;
    if (!(ss >> command)) {
      std::cerr << "Error extracting command" << std::endl;
      return;
    }
    map<string, void(MessageHandler::*)(Client&, stringstream&)>::const_iterator cmd_it = command_map_.find(command);
    if (cmd_it != command_map_.end()) {
        (this->*(cmd_it->second))(client, ss);
        if (command == "QUIT") 
          return;
    } else {
      message  = "421 " + client.nick_ + " " + command + " :Unknown command\r\n";
      client.add_message_out(message);
      // client.messages_outgoing_.append(message);
      std::cout << "Unknown command: " << command << std::endl;
    }
  }
  client.messages_incoming_.erase(0, start);
  server_.epoll_in_out(client.fd_);
  // cout << client.messages_outgoing_ << std::endl; // DEBUG
}

void MessageHandler::reply_ERR_NEEDMOREPARAMS(Client& client, const string& command) {
  if (!command.empty()) {
    string message  = "461 " + client.nick_ + " " + command + " :Not enough parameters\r\n";
    client.add_message_out(message);
  }
}

#include <arpa/inet.h> // For inet_ntoa

void MessageHandler::reply_ERR_USERNOTINCHANNEL(Client& client, const string& target_nick, const string& channel) {
  string message = "441 " + target_nick + " " + channel + " :They aren't on that channel\r\n";
  client.add_message_out(message);
}

void MessageHandler::reply_ERR_NOSUCHCHANNEL(Client& client, const string& channel_name) {
  // std::string server_address = inet_ntoa(server_.server_addr_.sin_addr);
  // std::string error_message = ":" + server_address + " 403 " + client.nick_ + " " + channel_name + " :Invalid channel name\r\n";
  std::string error_message = "403 " + client.nick_ + " " + channel_name + " :Invalid channel name\r\n";
  client.add_message_out(error_message);
}

string MessageHandler::create_message(Client& client, const string& command, const string& parameters, const string& message_content) {
  string message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " " + command + " " + parameters + " :" + message_content + "\r\n";
  return message;
}


void MessageHandler::client_not_registered(Client& client) {
  string message = "451 * :You have not registered\r\n";
  client.add_message_out(message);

  // client.messages_outgoing_.append(oss.str());
  // cout << oss.str() << std::endl;
  server_.add_to_close(&client);
}

void MessageHandler::command_CAP(Client& client, std::stringstream& /* message */) {
  if (client.state_ != WAITING_FOR_NICK) {
    client_not_registered(client);
    return;
  }
  std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
}

void MessageHandler::command_NICK(Client& client, std::stringstream& message) {
  if (client.state_ != WAITING_FOR_NICK && client.state_ != REGISTERED) {
    client_not_registered(client);
    return;
  }

  string nick;
  if (message >> nick) {
    client.nick_ = nick;
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, "NICK");
  }
  if (client.state_ == WAITING_FOR_NICK) {
    client.state_ = WAITING_FOR_USER;
  }

  std::cout << "Handling NICK command for client " << client.nick_<< std::endl;
}


void MessageHandler::command_USER(Client& client, std::stringstream& message) {
  if (client.state_ == REGISTERED) {
    string reply = "462 " + client.nick_ + " :You may not reregister\r\n";
    client.add_message_out(reply);
    
    // client.messages_outgoing_.append(reply);
    return;
  }
  
  if (client.state_ != WAITING_FOR_USER) {
    client_not_registered(client);
    return;
  }
  
  std::string username, user_mode, hostname, realname;

  if (!(message >> username >> user_mode >> hostname)) {
    reply_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }

  // Extract the realname which may contain spaces
  std::getline(message >> std::ws, realname);
  if (realname.empty()) {
    reply_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }
  if (client.state_ == WAITING_FOR_USER) {
    client.state_ = REGISTERED;
    cout << "State after User: " << client.state_ << std::endl;
  }


  client.username_ = username;
  // client.hostname_ = hostname;
  client.realname_ = realname;

  std::string reply;
  reply.reserve(128);

  reply = "001 " + client.nick_ + " :Welcome to the IRC server\r\n";
  client.add_message_out(reply);


  reply = "002 " + client.nick_ + " :Your host is " + inet_ntoa(server_.server_addr_.sin_addr) + ", running version 1.0\r\n";
  client.add_message_out(reply);
  

  reply = "003 " + client.nick_ + " :This server was created today\r\n";
  client.add_message_out(reply);

  reply = "004 " + client.nick_ + " " + inet_ntoa(server_.server_addr_.sin_addr) + " 1.0 o o\r\n";
  client.add_message_out(reply);

  reply = "005 " + client.nick_ + " :Some additional information\r\n";
  client.add_message_out(reply);

  std::cout << "Handling USER command for client " << client.username_ << std::endl;
}


void MessageHandler::command_PING(Client& client, std::stringstream& message) {
  if (client.state_ != REGISTERED) {
    client_not_registered(client);
    return;
  }
  string target;
  if (message >> target) {
    string reply = "PONG \r\n";
    client.add_message_out(reply);
  }
  else {
    reply_ERR_NEEDMOREPARAMS(client, "PING");
  }
}

void MessageHandler::command_QUIT(Client& client, std::stringstream& message) {
  std::string quit_message;
  std::getline(message >> std::ws, quit_message); // skip initial whitespace

  if (!quit_message.empty() && quit_message[0] == ':') {
    quit_message.erase(0, 1);
  }

  std::string broadcast_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " QUIT :" + quit_message + "\r\n";
  client.message_to_all_channels(client.fd_, broadcast_message);
  server_.delete_client(client.fd_);
}

void MessageHandler::command_JOIN(Client& client, std::stringstream& message) {
  if (client.state_ != REGISTERED) {
    client_not_registered(client);
    return;
  }
  std::string channels_str;
  std::string keys_str;

  // Extract the channels and keys from the message
  if (!(message >> channels_str)) {
    reply_ERR_NEEDMOREPARAMS(client, "JOIN");
    return;
  }
  std::getline(message, keys_str);

  // Split the channels and keys by comma
  std::vector<std::string> channels;
  std::vector<std::string> keys;
  std::stringstream ss_channels(channels_str);
  std::stringstream ss_keys(keys_str);

  std::string token;
  while (std::getline(ss_channels, token, ',')) {
    if (!token.empty()) {
      channels.push_back(token);
    }
  }
  while (std::getline(ss_keys, token, ',')) {
    if (!token.empty()) {
      keys.push_back(token);
    }
  }

  // Process each channel
  for (size_t i = 0; i < channels.size(); ++i) {
    std::string channel_name = channels[i];
    if (channel_name[0] != '#' && channel_name[0] != '&') {
      reply_ERR_NOSUCHCHANNEL(client, channel_name);
      continue;
    }

    std::map<std::string, Channel*>::iterator it = server_.channels_.find(channel_name);
    if (it == server_.channels_.end()) {
      server_.create_channel(channel_name, client);
    } else {
      server_.join_channel(channel_name, client);
    }
  }
}

void MessageHandler::command_PRIVMSG(Client& sender, std::stringstream& message) {
  if (sender.state_ != REGISTERED) {
    client_not_registered(sender);
    return;
  }
  std::string target;
  if (!(message >> target)) {
    reply_ERR_NEEDMOREPARAMS(sender, "PRIVMSG");
    return;
  }
  // Extract the remaining message content from the current position
  std::string message_content = message.str().substr(static_cast<std::string::size_type>(message.tellg()));
  if (message_content.empty()) {
    reply_ERR_NEEDMOREPARAMS(sender, "PRIVMSG");
    return;
  }

  // Remove leading whitespace and leading ':' if present
  size_t start = 0;
  while (start < message_content.size() && (message_content[start] == ' ' || message_content[start] == ':')) {
    ++start;
  }
  message_content.erase(0, start);

  // Target is a channel
  if (target[0] == '#' || target[0] == '&') {
    Channel* channel = server_.get_channel(target);
    if (channel == NULL) {
      reply_ERR_NOSUCHCHANNEL(sender, target);
      return;
    }

    // Construct the PRIVMSG message
    std::ostringstream oss;
    // cout << "Hostname: " << sender.hostname_ << std::endl;
    oss << ":" << sender.nick_ << "!" << sender.username_ << "@" << sender.hostname_ << " PRIVMSG " << target << " :" << message_content << "\r\n";

    // Broadcast the message to the channel
    channel->broadcast(sender.fd_, oss.str());
    // std::cout << "Broadcast: " << oss.str() << std::endl;

  } else {
    // Handle sending message to a user
    // Client* target_client = server_.get_client_by_nick(target);
    // if (target_client == NULL) {
    //   std::ostringstream oss;
    //   oss << "401 " << sender.nick_ << " " << target << " :No such nick/channel\r\n";
    //   sender.messages_outgoing_.append(oss.str());
    //   server_.epoll_in_out(sender.fd_);
    //   return;
    // }

    // // Construct the PRIVMSG message
    // std::ostringstream oss;
    // oss << ":" << sender.nick_ << "!" << sender.username_ << "@" << sender.hostname_ << " PRIVMSG " << target_client->nick_ << " :" << message_content << "\r\n";

    // // Send the message to the target client
    // target_client->messages_outgoing_.append(oss.str());
    // server_.epoll_in_out(target_client->fd_);
  }
}




void MessageHandler::command_MODE(Client& client, std::stringstream& message) {
  if (client.state_ != REGISTERED) {
    client_not_registered(client);
    return;
  }
  std::string target;
  if (!(message >> target)) {
    reply_ERR_NEEDMOREPARAMS(client, "MODE");
    return;
  }

  // Check if the target is a channel or a user
  if (target[0] == '#' || target[0] == '&') {
    // Handle channel mode
    handle_channel_mode(client, target, message);
  } else {
    // Handle user mode
    // (client, target, message);
  }
}

void MessageHandler::handle_channel_mode(Client& client, const std::string& channel_name, std::stringstream& message) {
  std::map<std::string, Channel*>::iterator it = server_.channels_.find(channel_name);
  if (it == server_.channels_.end()) {
    // Channel does not exist
    reply_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }

  Channel* channel = it->second;

  std::string mode_changes;
  if (!(message >> mode_changes)) {
    // No mode changes, just return the current mode
    std::ostringstream oss;
    oss << "324 " << client.nick_ << " " << channel_name << " +\r\n"; // Simplified, should include actual modes
    client.add_message_out(oss.str());
  } 
  else {
    // // Apply mode changes
    // std::ostringstream oss;
    // oss << ":" << client.nick_ << "!" << client.username_ << "@" << client.hostnamevoid MessageHandler::command_PASSWD(Client& client, std::stringstream& message) {
    // channel->broadcast(client.fd_, oss.str());
  }
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


void MessageHandler::command_PASS(Client& client, std::stringstream& message) {
  if (client.state_ != WAITING_FOR_PASS) {
    return;
  }

  std::string password;
  if (!(message >> password)) {
    reply_ERR_NEEDMOREPARAMS(client, "PASS");
    client_not_registered(client);
    return;
  }
  if (server_.password_ == password) {
    client.state_ = WAITING_FOR_NICK;
  } 
  else {
    std::string error_message = "464 * :Password incorrect\r\n";
    client.messages_outgoing_.append(error_message);
    client_not_registered(client);
    // server_.epoll_in_out(client.fd_);
  }
}

string MessageHandler::extract_message(stringstream& message) {
  std::string message_content;
  std::getline(message, message_content);
  // message.str().substr(static_cast<std::string::size_type>(message.tellg()));
  if (message_content.empty()) {
    return message_content;
  }

  // Remove leading whitespace and leading ':' if present
  size_t start = 0;
  while (start < message_content.size() && (message_content[start] == ' ' || message_content[start] == ':')) {
    ++start;
  }
  message_content.erase(0, start);
  return message_content;
}

// KICK #example bob :Spamming is not allowed



void MessageHandler::command_KICK(Client& client, std::stringstream& message) {
  if (client.state_ != REGISTERED) {
    return;
  }
  string channel_name;
  if (!(message >> channel_name)) {
    reply_ERR_NEEDMOREPARAMS(client, "KICK");
    return;
  }

  string target;
  if (!(message >> target)) {
    reply_ERR_NEEDMOREPARAMS(client, "KICK");
    return;
  }
  
  string message_content = extract_message(message);
  // if (message_content.empty()) {
  //   reply_ERR_NEEDMOREPARAMS(client, "KICK");
  //   return;
  // }

  Channel* channel;
  if (channel_name[0] == '#' || channel_name[0] == '&') {
  channel = server_.get_channel(channel_name);
    if (channel == NULL) {
      reply_ERR_NOSUCHCHANNEL(client, channel_name);
      return;
    }
  }
  else {
    // send response, channel name invalid
  }


  Client* client_to_kick = channel->get_client(target);
  if (!client_to_kick) {
    reply_ERR_USERNOTINCHANNEL(client, target, channel_name);
    return;
  }




  string parameters = channel_name + " " + target;

  string reply = create_message(client, "KICK", parameters, message_content);
  channel->broadcast(client.fd_, reply);

}


