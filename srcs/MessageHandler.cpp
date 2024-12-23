#include <iostream>
#include <cstdio>
#include <sys/epoll.h>
#include <string>
#include <arpa/inet.h>
#include <algorithm>
#include <cctype>
#include "MessageHandler.hpp"

using std::string;
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
  command_map_["INVITE"] = &MessageHandler::command_INVITE;
  command_map_["KICK"] = &MessageHandler::command_KICK;
  command_map_["TOPIC"] = &MessageHandler::command_TOPIC;
  command_map_["PART"] = &MessageHandler::command_PART;
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
    if (message.size() > 512) {
      REPLY_ERR_INPUTTOOLONG(client);
      start = end + 2;
      continue;
    }
    // cout << message << std::endl; // DEBUG
    start = end + 2;
    stringstream ss(message);
    string command;
    if (!(ss >> command)) {
      std::cerr << "Error extracting command" << std::endl;
      return;
    }
    command = server_.to_upper(command);
    std::map<string, void(MessageHandler::*)(Client&, stringstream&)>::const_iterator cmd_it = command_map_.find(command);
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

void MessageHandler::command_CAP(Client& client, stringstream& message) {
  string subcommand;
  message >> subcommand;

  if (subcommand == "LS") {
    string reply = "CAP * LS :echo-message\r\n";
    client.add_message_out(reply);
  } else if (subcommand == "REQ") {
    string capability;
    message >> capability;
    if (capability == ":echo-message") {
      string reply = "CAP * ACK :echo-message\r\n";
      client.add_message_out(reply);
    } else {
      string reply = "CAP * NAK :" + capability + "\r\n";
      client.add_message_out(reply);
    }
  } else if (subcommand == "END") {
    // Optional: Mark capabilities negotiation as complete
    string reply = "CAP * END\r\n";
    client.add_message_out(reply);
  } else {
    string reply = "CAP * NAK :" + subcommand + "\r\n";
    client.add_message_out(reply);
  }
  std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
}

void MessageHandler::command_NICK(Client& client, stringstream& message) {
  if (client.state_ != WAITING_FOR_NICK && client.state_ != REGISTERED) {
    ERR_NOTREGISTERED(client);
    return;
  }
  string nick;
  std::getline(message >> std::ws, nick);
  if (nick.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(client, "NICK");
    return;
  }
  if (nick.length() > 9) {
    REPLY_ERR_ERRONEUSNICKNAME(client, nick);
    return;
  }
  if (!isalpha(nick[0]) && nick[0] != '[' && nick[0] != ']' && nick[0] != '\\' && nick[0] != '^' && nick[0] != '_' && nick[0] != '{' && nick[0] != '|' && nick[0] != '}') {
    REPLY_ERR_ERRONEUSNICKNAME(client, nick);
    return;
  }
  for (size_t i = 1; i < nick.length(); ++i) {
    if (!isalnum(nick[i]) && nick[i] != '-' && nick[i] != '[' && nick[i] != ']' && nick[i] != '\\' && nick[i] != '^' && nick[i] != '_' && nick[i] != '{' && nick[i] != '|' && nick[i] != '}') {
      REPLY_ERR_ERRONEUSNICKNAME(client, nick);
      return;
    }
  }
  if (!server_.check_nick(nick)) {
    string reply = "433 " + client.nick_ + " " + nick + " :Nickname is already in use\r\n";
    client.add_message_out(reply);
    return;
  }
  string nick_reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " NICK :" + nick + "\r\n";
  client.broadcast_all_channels(nick_reply);

  client.nick_ = nick;
  if (client.state_ == WAITING_FOR_NICK) {
    client.state_ = WAITING_FOR_USER;
  }
  std::cout << "Handling NICK command for client " << client.nick_ << std::endl;
}

void MessageHandler::command_USER(Client& client, stringstream& message) {
  if (client.state_ == REGISTERED) {
    string reply = "462 " + client.nick_ + " :You may not reregister\r\n";
    client.add_message_out(reply);
    return;
  }
  if (client.state_ != WAITING_FOR_USER) {
    ERR_NOTREGISTERED(client);
    return;
  }
  string username, user_mode, hostname, realname;
  if (!std::getline(message >> std::ws, username, ' ') || username.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }
  if (!std::getline(message >> std::ws, user_mode, ' ') || user_mode.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }
  if (!std::getline(message >> std::ws, hostname, ' ') || hostname.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }
  if (!std::getline(message >> std::ws, realname) || realname.empty()) {
    REPLY_ERR_NEEDMOREPARAMS(client, "USER");
    return;
  }
  if (realname.at(0) == ':') {
    realname.erase(0, 1);
  }
  if (username.length() > 9) {
    REPLY_ERR_ERRONEUSNICKNAME(client, username);
    return;
  }
  for (size_t i = 0; i < username.length(); ++i) {
    if (!isalnum(username[i]) && username[i] != '-' && username[i] != '_' && username[i] != '.') {
      REPLY_ERR_ERRONEUSNICKNAME(client, username);
      return;
    }
  }
  if (hostname.length() > 63) {
    REPLY_ERR_ERRONEUSNICKNAME(client, hostname);
    return;
  }
  for (size_t i = 0; i < hostname.length(); ++i) {
    if (!isalnum(hostname[i]) && hostname[i] != '-' && hostname[i] != '.' && hostname[i] != '*') {
      REPLY_ERR_ERRONEUSNICKNAME(client, hostname);
      return;
    }
  }
  if (realname.length() > 50) {
    REPLY_ERR_ERRONEUSNICKNAME(client, realname);
    return;
  }
  client.state_ = REGISTERED;
  client.username_ = username;
  client.realname_ = realname;
  string reply;
  reply.reserve(128);
  reply += "001 " + client.nick_ + " :Welcome to the IRC server\r\n";
  reply += "002 " + client.nick_ + " :Your host is " + inet_ntoa(server_.server_addr_.sin_addr) + ", running version 1.0\r\n";
  reply += "003 " + client.nick_ + " :This server was created today\r\n";
  reply += "004 " + client.nick_ + " " + inet_ntoa(server_.server_addr_.sin_addr) + " 1.0 - itkol\r\n";
  client.add_message_out(reply);
  std::cout << "Handling USER command for client " << client.username_ << std::endl;
  }

void MessageHandler::command_PING(Client& client, stringstream& message) {
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

void MessageHandler::command_QUIT(Client& client, stringstream& message) {
  string quit_message;
  std::getline(message >> std::ws, quit_message);
  if (!quit_message.empty() && quit_message[0] == ':') {
    quit_message.erase(0, 1);
  }
  string broadcast_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " QUIT :" + quit_message + "\r\n";
  client.message_to_all_channels(broadcast_message);
  server_.delete_client(client.fd_);
}

void MessageHandler::command_JOIN(Client& client, stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string channel_name;
  string key;
  if (!(message >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "JOIN");
    return;
  }
  std::getline(message >> std::ws, key);
  if (channel_name[0] != '#') { // Only support local channels prefixed with '#'
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
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
  string mode = channel->get_mode();
  if (mode.find('i') != string::npos && !channel->is_invited(client.fd_)) {
    REPLY_ERR_INVITEONLYCHAN(client, channel_name);
    return;
  }
  if (mode.find('k') != string::npos && !channel->check_key(key)) {
    REPLY_ERR_BADCHANNELKEY(client, channel_name);
    return;
  }
  if (channel->channel_full()) {
    REPLY_ERR_CHANNELISFULL(client, channel_name);
    return;
  }
  channel->add_client(client);
}

void MessageHandler::command_PRIVMSG(Client& sender, stringstream& message) {
  if (!client_registered(sender)) {
    return;
  }
  string target;
  if (!(message >> target)) {
    REPLY_ERR_NORECIPIENT(sender, "PRIVMSG");
    return;
  }
  string message_content;
  std::getline(message >> std::ws, message_content);
  if (message_content.empty()) {
    REPLY_ERR_NOTEXTTOSEND(sender);
    return;
  }
  if (!message_content.empty() && message_content[0] == ':') {
    message_content.erase(0, 1);
  }
  if (target[0] == '#' || target[0] == '&') {
    Channel* channel = server_.get_channel(target);
    if (target[0] == '&' || channel == NULL) {
      REPLY_ERR_NOSUCHCHANNEL(sender, target);
      return;
    }
    if (!(channel->is_on_channel(sender.fd_))) {
      REPLY_ERR_USERNOTINCHANNEL(sender, target);
      return;
    }
    string full_message = ":" + sender.nick_ + "!" + sender.username_ + "@" + sender.hostname_ + " PRIVMSG " + target + " :" + message_content + "\r\n";
    channel->broadcast(full_message);
  } 
  else {
    Client* target_client = server_.get_client(target);
    if (target_client == NULL) {
      REPLY_ERR_NOSUCHNICK(sender, target);
      return;
    }
    string full_message = ":" + sender.nick_ + "!" + sender.username_ + "@" + sender.hostname_ + " PRIVMSG " + target_client->nick_ + " :" + message_content + "\r\n";
    sender.add_message_out(full_message);
    target_client->add_message_out(full_message);
    server_.epoll_in_out(target_client->fd_);
  }
}

void MessageHandler::command_MODE(Client& client, stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string channel_name;
  if (!(message >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "MODE");
    return;
  }
  if (channel_name[0] != '#' && channel_name[0] != '&') {
    string reply = "501 " + client.nick_ + " :User mode changes are not supported\r\n";
    client.add_message_out(reply);
    return;     
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }
  string mode_changes;
  if (!(message >> mode_changes)) {
    std::cout << "current mode " << channel->get_mode() << std::endl;
    string reply = "324 " + client.nick_ + " " + channel_name + " +" + channel->get_mode() + "\r\n";
    client.add_message_out(reply);
    return;
  }
  if (!channel->is_operator(client.fd_)) {
    REPLY_ERR_CHANOPRIVSNEEDED(client, channel->get_name());
    return;
  }
  std::cout << "mode changes: " << mode_changes << std::endl;
  if (mode_changes.empty() || (mode_changes.at(0) != '+' && mode_changes.at(0) != '-')) {
    string reply = "461 " + client.nick_ + " " + channel_name + " :Invalid mode change format\r\n";
    client.add_message_out(reply);
    return;
  }

  bool add_mode = (mode_changes[0] == '+');
  for (size_t i = 1; i < mode_changes.size(); ++i) {
    char mode = mode_changes[i];
    switch (mode) {
      case 'i':
        if (add_mode) {
          channel->set_mode("i");
        } else {
          channel->set_mode("-i");
        }
        break;
      case 't':
        if (add_mode) {
          channel->set_mode("t");
        } else {
          channel->set_mode("-t");
        }
        break;
      case 'k': {
        string key;
        if (add_mode) {
          if (!(message >> key)) {
            REPLY_ERR_NEEDMOREPARAMS(client, "MODE");
            return;
          }
          channel->set_mode("k");
          channel->set_key(key);
        } else {
          channel->remove_key();
        }
        break;
      }
      case 'o': {
        string nick;
        if (!(message >> nick)) {
          REPLY_ERR_NEEDMOREPARAMS(client, "MODE");
          return;
        }
        Client* target = channel->get_client(nick);
        if (!target) {
          REPLY_ERR_NOSUCHNICK(client, nick);
          return;
        }
        if (add_mode) {
          channel->add_operator(*target);
          std::string reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " MODE " + channel->get_name() + " +o " + target->nick_ + "\r\n";
          channel->broadcast(reply);
        } else {
          channel->remove_operator(target->fd_);
          std::string reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " MODE " + channel->get_name() + " -o " + target->nick_ + "\r\n";
          channel->broadcast(reply);
        }
        break;
      }
      case 'l': {
        ushort limit;
        if (add_mode) {
          if (!(message >> limit || limit > 100)) {
            string reply = "461 " + client.nick_ + " MODE :Invalid user limit\r\n";
            client.add_message_out(reply);
            return;
          }
          channel->set_mode("l");
          channel->set_user_limit(limit);
        } else {
          channel->remove_user_limit();
        }
        break;
      }
      default:
        string reply = "472 " + client.nick_ + " " + mode + " :is unknown mode char to me\r\n";
        client.add_message_out(reply);
        return;
    }
  }
  string reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " MODE " + channel_name + " " + mode_changes + "\r\n";
  channel->broadcast(reply);
}

void MessageHandler::command_PASS(Client& client, stringstream& message) {
  if (client.state_ != WAITING_FOR_PASS) {
    return;
  }

  string password;
  if (!(message >> password)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "PASS");
    ERR_NOTREGISTERED(client);
    return;
  }
  if (server_.check_password(password)) {
    client.state_ = WAITING_FOR_NICK;
  }
  else {
    string error_message = "464 * :Password incorrect\r\n";
    client.add_message_out(error_message);
    ERR_NOTREGISTERED(client);
  }
}

string MessageHandler::extract_message(stringstream& message) {
  string message_content;
  std::getline(message, message_content);
  if (message_content.empty()) {
    return message_content;
  }
  size_t start = 0;
  while (start < message_content.size() && (message_content[start] == ' ' || message_content[start] == ':')) {
    ++start;
  }
  message_content.erase(0, start);
  return message_content;
}

void MessageHandler::command_KICK(Client& client, stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string channel_name, target;
  if (!(message >> channel_name >> target)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "KICK");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#')) {
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
  string reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " KICK " + channel_name + " " + target + " :" + message_content + "\r\n";
  channel->broadcast(reply);
  channel->remove_client(*client_to_kick);
  client_to_kick->remove_channel(channel->get_name());
}

void MessageHandler::command_INVITE(Client& client, stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string target, channel_name;
  if (!(message >> target >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "INVITE");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#')) {
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
  Client* invitee = server_.get_client(target);
  if (!invitee) {
    REPLY_ERR_NOSUCHNICK(client, target);
    return;
  }
  if (channel->is_on_channel(invitee->fd_)) {
    REPLY_ERR_USERONCHANNEL(client, target, channel->get_name());
    return;
  }
  channel->invite_client(client, *invitee);
  string reply = "341 " + client.nick_ + " " + target + " " + channel_name + "\r\n";
  client.add_message_out(reply);
  string invite_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " INVITE " + target + " :" + channel_name + "\r\n";
  invitee->add_message_out(invite_message);
  server_.epoll_in_out(invitee->fd_);
}

void MessageHandler::command_TOPIC(Client& client, stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string channel_name, topic;
  if (!(message >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "TOPIC");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#')) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }
  if (!channel->is_on_channel(client.fd_)) {
    REPLY_ERR_NOTONCHANNEL(client, channel_name);
    return;
  }
  std::getline(message >> std::ws, topic);
  if (topic.empty()) {
    topic = channel->get_topic();
    string reply;
    if (topic.empty()) {
      reply = "331 " + client.nick_ + " " + channel->get_name() + " :No topic is set\r\n";
    } else {
      reply = "332 " + client.nick_ + " " + channel->get_name() + " :" + topic + "\r\n";
    }
    client.add_message_out(reply);
    return;
  }
  string mode = channel->get_mode();
  if (mode.find('t') == string::npos || channel->is_operator(client.fd_)) {
    channel->modify_topic(client, topic);
    return;
  }
  REPLY_ERR_CHANOPRIVSNEEDED(client, channel_name);
}

void MessageHandler::command_PART(Client& client, stringstream& message) {
  if (!client_registered(client)) {
    return;
  }
  string channel_name;
  if (!(message >> channel_name)) {
    REPLY_ERR_NEEDMOREPARAMS(client, "PART");
    return;
  }
  Channel* channel = server_.get_channel(channel_name);
  if (!channel || (channel_name[0] != '#')) {
    REPLY_ERR_NOSUCHCHANNEL(client, channel_name);
    return;
  }
  if (!channel->is_on_channel(client.fd_)) {
    REPLY_ERR_NOTONCHANNEL(client, channel_name);
    return;
  }
  string part_message;
  message >> part_message;
  string full_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " PART " + channel_name + " " + part_message + "\r\n";
  channel->broadcast(full_message);
  channel->remove_client(client);
  client.remove_channel(channel->get_name());
}