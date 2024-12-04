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
  command_map_["TOPIC"] = &MessageHandler::command_TOPIC;
}

MessageHandler::~MessageHandler() {
  command_map_.clear();
}

void MessageHandler::send_messages(Client& client) {
  size_t length = client.messages_outgoing_.length();
  ssize_t bytes_sent = send(client.fd_, client.messages_outgoing_.c_str(), length, MSG_NOSIGNAL);
  // if (bytes_sent > 0) {
  //   cout << client.messages_outgoing_.substr(0, static_cast<size_t>(bytes_sent)) << std::endl; // DEBUG
  // }
  if (bytes_sent == static_cast<ssize_t>(length)) { // all messages have been sent
    client.messages_outgoing_.clear();
    server_.epoll_in(client.fd_);
  }
  else if (bytes_sent > 0) { // partial send, try again next loop
    client.messages_outgoing_.erase(0, static_cast<size_t>(bytes_sent));
  }
  else if (bytes_sent == -1 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
    perror("Error sending message to client");
    server_.delete_client(client.fd_);
  }
}

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
      std::cout << "Unknown command: " << command << std::endl;
    }
  }
  client.messages_incoming_.erase(0, start);
  server_.epoll_in_out(client.fd_);
  // cout << client.messages_outgoing_ << std::endl; // DEBUG
}

string MessageHandler::create_message(Client& client, const string& command, const string& parameters, const string& message_content) {
  string message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " " + command + " " + parameters + " :" + message_content + "\r\n";
  return message;
}

bool MessageHandler::client_registered(Client& client) {
  if (client.state_ != REGISTERED) {
    ERR_NOTREGISTERED(client);
    return false;
  }
  return true;
}


void MessageHandler::ERR_NOTREGISTERED(Client& client) {
  string message = "451 * :You have not registered\r\n";
  client.add_message_out(message);
  server_.add_to_close(&client);
}

void MessageHandler::command_CAP(Client& client, std::stringstream& /* message */) {
  if (client.state_ != WAITING_FOR_NICK) {
    ERR_NOTREGISTERED(client);
    return;
  }
  std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
}

void MessageHandler::command_NICK(Client& client, std::stringstream& message) {
  if (client.state_ != WAITING_FOR_NICK && client.state_ != REGISTERED) {
    ERR_NOTREGISTERED(client);
    return;
  }

  string nick;
  if (message >> nick) {
    client.nick_ = nick;
  }
  else {
    REPLY_ERR_NEEDMOREPARAMS(client, "NICK");
    return;
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
    return;
  }
  
  if (client.state_ != WAITING_FOR_USER) {
    ERR_NOTREGISTERED(client);
    return;
  }
  
  std::string username, user_mode, hostname, realname;

  if (!(message >> username >> user_mode >> hostname)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }

  // Extract the realname which may contain spaces
  std::getline(message >> std::ws, realname);
  if (realname.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(client, "USER");
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
  
  reply.append("001 ").append(client.nick_).append(" :Welcome to the IRC server\r\n");
  reply.append("002 ").append(client.nick_).append(" :Your host is ").append(inet_ntoa(server_.server_addr_.sin_addr)).append(", running version 1.0\r\n");
  reply.append("003 ").append(client.nick_).append(" :This server was created today\r\n");
  reply.append("004 ").append(client.nick_).append(" ").append(inet_ntoa(server_.server_addr_.sin_addr)).append(" 1.0 o o\r\n");
  reply.append("005 ").append(client.nick_).append(" :Some additional information\r\n");
  client.add_message_out(reply);

  std::cout << "Handling USER command for client " << client.username_ << std::endl;
}


void MessageHandler::command_PING(Client& client, std::stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string target;
  if (message >> target) {
    string reply = "PONG \r\n";
    client.add_message_out(reply);
  }
  else {
    REPLY_ERR_NEEDMOREPARAMS(client, "PING");
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
  if (!client_registered(client)) {
    return;
  }
  std::string channels_str;
  std::string keys_str;

  // Extract the channels and keys from the message
  if (!(message >> channels_str)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "JOIN");
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
      REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
      continue;
    }

    if (client.chan_limit_reached()) {
      REPLY_ERR_TOOMANYCHANNELS(client, channel_name);
      return;
    }
    Channel* channel = server_.get_channel(channel_name);
    if (channel == NULL) {
      server_.create_channel(channel_name, client);
      return;
    }
    channel->add_client(client);
  }
}

void MessageHandler::command_PRIVMSG(Client& sender, std::stringstream& message) {
  if (!client_registered(sender)) {
    return;
  }
  std::string target;
  if (!(message >> target)) {
    REPLY_ERR_NEEDMOREPARAMS(sender, "PRIVMSG");
    return;
  }
  // Extract the remaining message content from the current position
  std::string message_content = message.str().substr(static_cast<std::string::size_type>(message.tellg()));
  if (message_content.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(sender, "PRIVMSG");
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
      REPLY_ERR_NOSUCHCHANNEL(sender, target);
      return;
    }

  if (!(channel->is_on_channel(sender.fd_))) {
    REPLY_ERR_USERNOTINCHANNEL(sender, target);
    return;
  }
  std::ostringstream oss;
  oss << ":" << sender.nick_ << "!" << sender.username_ << "@" << sender.hostname_ << " PRIVMSG " << target << " :" << message_content << "\r\n";
  channel->broadcast(sender.fd_, oss.str());
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
  if (!client_registered(client)) {
    return;
  }
  std::string channel_name;
  if (!(message >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "MODE");
    return;
  }
  if (channel_name[0] != '#' && channel_name[0] != '&') {
    // User mode changes are not supported
    std::string reply = "501 " + client.nick_ + " :User mode changes are not supported\r\n";
    client.add_message_out(reply);
    return;     
  }

  Channel* channel = server_.get_channel(channel_name);
  if (!channel) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }

  std::string mode_changes;
  if (!(message >> mode_changes)) {
    std::cout << "current mode " << channel->get_mode() << std::endl;
    std::string reply = "324 " + client.nick_ + " " + channel_name + " +" + channel->get_mode() + "\r\n";
    client.add_message_out(reply);
    return;
  }

  std::cout << "mode changes: " << mode_changes << std::endl;
  if (mode_changes.empty() || (mode_changes.at(0) != '+' && mode_changes.at(0) != '-')) {
    // Invalid mode change format, handle the error
    std::string reply = "461 " + client.nick_ + " " + channel_name + " :Invalid mode change format\r\n";
    client.add_message_out(reply);
    return;
  }
  channel->set_mode(mode_changes);

  // Apply mode changes
  std::string reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " MODE " + channel_name + " " + mode_changes + "\r\n";
  channel->broadcast(client.fd_, reply);
}

void MessageHandler::command_PASS(Client& client, std::stringstream& message) {
  if (client.state_ != WAITING_FOR_PASS) {
    return;
  }

  std::string password;
  if (!(message >> password)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "PASS");
    ERR_NOTREGISTERED(client);
    return;
  }
  if (server_.check_password(password)) {
    client.state_ = WAITING_FOR_NICK;
  }
  else {
    std::string error_message = "464 * :Password incorrect\r\n";
    client.add_message_out(error_message);
    ERR_NOTREGISTERED(client);
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
  if (!client_registered(client)) {
    return;
  }
  std::string channel_name, target;
  if (!(message >> channel_name >> target)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "KICK");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#' && channel_name[0] != '&')) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }
  if (!channel->is_on_channel(client.fd_)) {
    REPLY_ERR_NOTONCHANNEL(client, channel_name);
    return;
  }
  if (!channel->is_operator(client.fd_)) {
    REPLY_ERR_CHANOPRIVSNEEDED(client, channel_name);
    return;
  }
  Client* client_to_kick = channel->get_client(target);
  if (!client_to_kick) {
    REPLY_ERR_USERNOTINCHANNEL(client, channel_name);
    return;
  }
  string message_content = extract_message(message);
  if (message_content.empty()) {
    message_content = "No reason specified";
  }
  string parameters = channel_name + " " + target;
  string reply = create_message(client, "KICK", parameters, message_content);
  channel->broadcast(client.fd_, reply);
  channel->remove_client(client_to_kick->fd_);
}

void MessageHandler::command_INVITE(Client& client, std::stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  std::string channel_name, target;
  if (!(message >> channel_name >> target)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "INVITE");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#' && channel_name[0] != '&')) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }
  if (!channel->is_on_channel(client.fd_)) {
    REPLY_ERR_NOTONCHANNEL(client, channel_name);
    return;
  }
  if (!channel->is_operator(client.fd_)) {
    REPLY_ERR_CHANOPRIVSNEEDED(client, channel_name);
    return;
  }
}

void MessageHandler::command_TOPIC(Client& client, std::stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  std::string channel_name, topic;
  if (!(message >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "TOPIC");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#' && channel_name[0] != '&')) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }

  std::getline(message >> std::ws, topic);
  if (topic.empty()) {
    topic = channel->get_topic();
    std::string reply;
    if (topic.empty()) {
      reply = "331 " + client.nick_ + " " + channel->get_name() + " :No topic is set\r\n";
    } else {
      reply = "332 " + client.nick_ + " " + channel->get_name() + " :" + topic + "\r\n";
    }
    client.add_message_out(reply);
    return;
  }
  std::string mode = channel->get_mode();
  if (mode.find('t') == std::string::npos || channel->is_operator(client.fd_)) {
    channel->modify_topic(client, topic);
    return;
  }
  REPLY_ERR_CHANOPRIVSNEEDED(client, channel_name);
}
