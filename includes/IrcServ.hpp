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

class MessageHandler;

class IrcServ {
private:
  int port_;
  int server_fd_;
  int ep_fd_;
  std::string password_;
  static IrcServ* instance_;
  

  static void signal_handler(int signal);
  static void set_non_block(int sock_fd);
  void close_client_fd(int fd);

  void initializeServerAddr();
  void register_signal_handlers();
  void event_loop();


public:
  sockaddr_in server_addr_;
  std::map<int, Client*> clients_; 
  MessageHandler* message_handler_;

  IrcServ(int port);
  ~IrcServ();
  IrcServ(int port, std::string password);
  void close_socket(int fd);

  // IrcServ(const IrcServ &other);
  // IrcServ &operator=(const IrcServ &other);

  void start();
  void cleanup();

  // int SetPort(int port);
};

#endif
