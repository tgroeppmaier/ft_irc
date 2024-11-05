#include "Channel.hpp"

using std::string;
using std::map;


Channel::Channel(std::string name, Client& admin) : name_(name) {
  admins_[admin.fd_] = &admin;
}

Channel::~Channel() {}
