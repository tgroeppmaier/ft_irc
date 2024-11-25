#ifndef CHANNEL_HPP
#define CHANNEL_HPP

// #include <iostream>
// #include <sstream>
// #include <vector>
#include <map>
#include <string>
#include <deque>
#include "Client.hpp"
#include "IrcServ.hpp"
// #include "IrcServ.hpp"

class Client;
class IrcServ;

class Channel {
private:
  IrcServ& server_;
  std::string password_;
  int user_limit_;
  std::string name_;


public:
  Channel(IrcServ& server, const std::string name, Client& admin);
  ~Channel();
  // vector<int> users_;
  // vector<int> operators_;
  std::map<int, Client*> clients_;
  std::map<int, Client*> admins_;

  void join_message_to_all(Client& client);
  void broadcast(int sender_fd, const std::string& message);
  void remove_client(Client& client);
  // std::deque<std::pair<int, std::string> > get_users();

};

#endif