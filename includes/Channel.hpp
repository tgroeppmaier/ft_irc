#ifndef CHANNEL_HPP
#define CHANNEL_HPP

// #include <iostream>
// #include <sstream>
// #include <vector>
#include <map>
#include <string>
#include "Client.hpp"
// #include "IrcServ.hpp"


class Channel {
private:
  std::string name_;
  
public:
  Channel(std::string name, Client& admin);
  ~Channel();
  std::map<int, Client*> clients_;
  std::map<int, Client*> admins_;

};

#endif