#ifndef IRCSERV_HPP
#define IRCSERV_HPP

#include <map>
#include <queue>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/epoll.h>
#include <string>
#include <cerrno>
#include <cstdlib>
#include "MessageHandler.hpp"
#include "Client.hpp"

struct UnsentMessage {
    int fd;
    size_t length;
    const char* message;
};

class IrcServ {
private:
  int port_;
  int server_fd_;
  std::string password_;
  sockaddr_in server_addr_;
  static IrcServ* instance_;
  int ep_fd_;
  deque<UnsentMessage> unsent_messages_;
  
  // MessageHandler message_handler_;

  static void signal_handler(int signal);
  static void set_non_block(int sock_fd);
  void close_client_fd(int fd);

  void initializeServerAddr();
  void register_signal_handlers();
  void event_loop();

  void send_message(Client& client, const char* message, size_t length);


  // Command handling functions
  static void command_CAP(Client& client, vector<string>& arguments);
  static void command_NICK(Client& client, vector<string>& arguments);
  static void command_USER(Client& client, vector<string>& arguments);

  // Map of command strings to function pointers



public:
  std::map<std::string, void(*)(Client&, vector<string>& arguments)> command_map_;
  std::map<int, Client*> clients_; 

  IrcServ(int port);
  IrcServ(int port, std::string password);
  // IrcServ(const IrcServ &other);
  // IrcServ &operator=(const IrcServ &other);
  // ~IrcServ();

  void start();
  void cleanup();

  // int SetPort(int port);
};

#endif
