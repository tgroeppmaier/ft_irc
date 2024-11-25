#include "Channel.hpp"

using std::string;
using std::map;


Channel::Channel(IrcServ& server, const std::string name, Client& admin) :  server_(server), name_(name) {
  admins_[admin.fd_] = &admin;
  clients_[admin.fd_] = &admin;
}

Channel::~Channel() {}

void Channel::join_message_to_all(Client& client) {
  string join_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " JOIN " + name_ + "\r\n";
  map<int, Client*>::const_iterator it = clients_.begin();
  for (; it != clients_.end(); ++it) {
    it->second->messages_outgoing_.append(join_message);
    server_.epoll_in_out(it->first);
  }
}

void Channel::broadcast(int sender_fd, const std::string& message) {
  std::map<int, Client*>::const_iterator it = clients_.begin();
  for (; it != clients_.end(); ++it) {
    if (it->first != sender_fd) { // Skip the sender
      it->second->add_message_out(message);
      // it->second->messages_outgoing_.append(message);
      server_.epoll_in_out(it->first);
    }
  }
}

void Channel::remove_client(Client& client) {
  map<int, Client*>::iterator it = clients_.find(client.fd_);
  if (it != clients_.end() && it->second == &client) {
    clients_.erase(it);
  }
}


// std::deque<std::pair<int, std::string> > Channel::get_users() {

// }

