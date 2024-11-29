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

#define USER_LIMIT 1

class Channel {
private:
  IrcServ& server_;
  int user_limit_;
  int user_count_;
  std::string name_;
  std::string password_;
  std::map<int, Client*> clients_;


public:
  Channel(IrcServ& server, const std::string name, Client& admin);
  ~Channel();
  std::map<int, Client*> admins_;

  // void join_message_to_all(Client& client);
  void broadcast(int sender_fd, const std::string& message);
  void add_client(Client& client);
  void add_operator(Client& client);
  void remove_client(int fd);
  Client* get_client(const std::string& name);

  bool is_operator(int fd);
  bool is_on_channel(int fd);
  // std::deque<std::pair<int, std::string> > get_users();

};

#endif