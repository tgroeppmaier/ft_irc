#include "Channel.hpp"

using std::string;
using std::map;


Channel::Channel(const std::string name, Client& admin) : name_(name) {
  admins_[admin.fd_] = &admin;
  clients_[admin.fd_] = &admin;
}

Channel::~Channel() {}
