#include "Channel.hpp"
#include <algorithm>
#include <cctype>

using std::string;
using std::map;


Channel::Channel(IrcServ& server, const std::string name, Client& client) :  server_(server), user_limit_(100), name_(name) {
  operators_[client.fd_] = &client;
  clients_[client.fd_] = &client;
}

Channel::~Channel() {}

// void Channel::join_message_to_all(Client& client) {
//   string join_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " JOIN " + name_ + "\r\n";
//   map<int, Client*>::const_iterator it = clients_.begin();
//   for (; it != clients_.end(); ++it) {
//     (*it).second->add_message_out(join_message);
//     // it->second->messages_outgoing_.append(join_message);
//     server_.epoll_in_out(it->first);
//   }
// }

void Channel::broadcast(int sender_fd, const std::string& message) {
  std::map<int, Client*>::const_iterator it = clients_.begin();
  for (; it != clients_.end(); ++it) {
    // if (it->first != sender_fd) { // Skip the sender
      it->second->add_message_out(message);
      server_.epoll_in_out(it->first);
    // }
  }
}

void Channel::add_client(Client& client) {
  client.add_channel(name_, this);
  clients_[client.fd_] = &client;
  invite_list_.erase(client.fd_);

  string join_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " JOIN " + name_ + "\r\n";
  broadcast(client.fd_, join_message);

  std::string reply;
  reply.reserve(128);
  if (topic_.empty()) {
    reply = "331 " + client.nick_ + " " + name_ + " :No topic is set\r\n";
  } else {
    reply = "332 " + client.nick_ + " " + name_ + " :" + topic_ + "\r\n";
  }
  string users = get_users();
  reply += "353 " + client.nick_ + " = " + name_ + " :" + users + "\r\n";
  reply += "366 " + client.nick_ + " " + name_ + " :End of /NAMES list\r\n";
  client.add_message_out(reply);
}

void Channel::add_operator(Client& client) {
  operators_[client.fd_] = &client;
}

void Channel::remove_operator(int fd) {
  operators_.erase(fd);
}

void Channel::remove_client(Client& client) {
  clients_.erase(client.fd_);
  operators_.erase(client.fd_);
  invite_list_.erase(client.fd_);
}

void Channel::remove_client_invite_list(int fd) {
  invite_list_.erase(fd);
}

void Channel::invite_client(Client& inviter, Client& invitee) {
  invite_list_.insert(invitee.fd_);
  string message = ":" + inviter.nick_ + "!" + inviter.username_ + "@" + inviter.hostname_ + " INVITE " + invitee.nick_ + " :" + name_ + "\r\n";
  broadcast(inviter.fd_, message);
}

bool Channel::is_invited(int fd) {
  return invite_list_.find(fd) != invite_list_.end();
}

Client* Channel::get_client(const std::string& name) {
  std::string upper_name = server_.to_upper(name);
  for (std::map<int, Client*>::const_iterator it = clients_.begin(); it != clients_.end(); ++it) {
    if (server_.to_upper(it->second->nick_) == upper_name) {
      return it->second;
    }
  }
  return NULL;
}

bool Channel::is_operator(int fd) {
  if (operators_.find(fd) == operators_.end()) {
    return false;
  }
  return true;
}

bool Channel::is_on_channel(int fd) {
  if (clients_.find(fd) == clients_.end()) {
    // std::cout << "is not on channel" << std::endl;
    // std::cout << "client fd: " << fd << std::endl;
    // std::cout << "All client fds on this channel:" << std::endl;
    // for (std::map<int, Client*>::const_iterator it = clients_.begin(); it != clients_.end(); ++it) {
    //   std::cout << it->first << std::endl;
    // }
    return false;
  }
  return true;
}

void Channel::set_mode(const std::string& mode) {
  bool add_mode = true;
  for (std::string::const_iterator it = mode.begin(); it != mode.end(); ++it) {
    char c = *it;
    if (c == '+') {
      add_mode = true;
    } 
    else if (c == '-') {
      add_mode = false;
    } 
    else {
      if (add_mode) {
        modes_.insert(c);
      } 
      else {
        modes_.erase(c);
      }
    }
  }
}

void Channel::set_key(const std::string& key) {
  key_ = key;
}

void Channel::remove_key() {
  modes_.erase('k');
  key_.clear();
}

bool Channel::check_key(const std::string& key) {
  return key == key_;
}

bool Channel::channel_full() {
  return clients_.size() >= user_limit_;
}

void Channel::set_user_limit(ushort limit) {
  user_limit_ = limit;
}

void Channel::remove_user_limit() {
  user_limit_ = 0;
}

void Channel::modify_topic(Client& client, const std::string& topic) {
  std::string reply;
  if (topic.empty()) {
    topic_.clear();
    reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " TOPIC " + name_ + " :\r\n";
  } else {
    topic_ = topic;
    reply = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " TOPIC " + name_ + " :" + topic + "\r\n";
  }
  broadcast(client.fd_, reply);
}

string Channel::get_mode() {
  string mode_string;
  std::set<char>::iterator it = modes_.begin();
  for (; it != modes_.end(); ++it) {
    char c = *it;
    mode_string.push_back(c);
  }
  return mode_string;
}

string Channel::get_name() {
  return name_;
}

string Channel::get_topic() {
  return topic_;
}

string Channel::get_users() {
  string users;
  std::map<int, Client*>::const_iterator it = clients_.begin();
  for (; it != clients_.end(); ++it) {
    if (!users.empty()) {
      users += ' ';
    }
    if (is_operator(it->second->fd_)) {
      users += '@';
    }
    users += it->second->nick_;
  }
  return users;
}






// std::deque<std::pair<int, std::string> > Channel::get_users() {

// }

