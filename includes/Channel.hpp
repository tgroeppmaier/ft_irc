#ifndef CHANNEL_HPP
#define CHANNEL_HPP

// #include <iostream>
// #include <sstream>
// #include <vector>
#include <map>
#include <string>
#include <set>
#include "Client.hpp"
#include "IrcServ.hpp"
// #include "IrcServ.hpp"

class Client;
class IrcServ;

// #define MAX_USERS_PER_CHANNEL 3

class Channel {
private:
  IrcServ& server_;
  int user_limit_;
  int user_count_;
  std::string name_;
  std::string password_;
  std::string topic_;
  std::map<int, Client*> clients_;
  std::map<int, Client*> operators_;
  std::set<char> modes_;

public:
  Channel(IrcServ& server, const std::string name, Client& client);
  ~Channel();

  // void join_message_to_all(Client& client);
  void broadcast(int sender_fd, const std::string& message);
  void add_client(Client& client);
  void add_operator(Client& client);
  void remove_client(int fd);
  Client* get_client(const std::string& name);

  bool is_operator(int fd);
  bool is_on_channel(int fd);
  void set_mode(const std::string& modes);
  void modify_topic(Client& client, const std::string& topic);
  std::string get_mode();
  std::string get_name();
  std::string get_topic();
  std::string get_users();
  // std::deque<std::pair<int, std::string> > get_users();

};

#endif