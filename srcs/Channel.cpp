#include "Channel.hpp"


using std::string;
using std::map;


Channel::Channel(IrcServ& server, const std::string name, Client& client) :  server_(server), name_(name) {
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

void Channel::remove_client(int fd) {
  clients_.erase(fd);
  operators_.erase(fd);
}

Client* Channel::get_client(const std::string& name) {
  for (map<int, Client*>::const_iterator it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->second->nick_ == name) {
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
    std::cout << "is not on channel" << std::endl;
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

std::string Channel::get_users() {
  std::string users;
  std::map<int, Client*>::const_iterator it = clients_.begin();
  for (; it != clients_.end(); ++it) {
    if (!users.empty()) {
      users += ' ';
    }
    users += it->second->nick_;
  }
  return users;
}






// std::deque<std::pair<int, std::string> > Channel::get_users() {

// }

