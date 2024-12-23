#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <stdio.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>

#include "IrcServ.hpp"

using std::string; using std::cout; using std::endl; using std::map;
IrcServ* IrcServ::instance_ = NULL;

IrcServ::IrcServ(int port) :
  message_handler_(new MessageHandler(*this)),
  channels_(),
  clients_to_close(),
  clients_(),
  password_(),
  hostname_(),
  port_(port),
  server_fd_(0),
  ep_fd_(0) {
  initializeServerAddr();
  instance_ = this;
}

IrcServ::IrcServ(int port, std::string password) :
  message_handler_(new MessageHandler(*this)),
  channels_(),
  clients_to_close(),
  clients_(),
  password_(password),
  hostname_(),
  port_(port),
  server_fd_(0),
  ep_fd_(0) {
  initializeServerAddr();
  instance_ = this;
}

// Copy constructor
IrcServ::IrcServ(const IrcServ &other) :
  message_handler_(new MessageHandler(*this)),
  channels_(other.channels_),
  clients_to_close(other.clients_to_close),
  clients_(other.clients_),
  password_(other.password_),
  hostname_(other.hostname_),
  port_(other.port_),
  server_fd_(other.server_fd_),
  ep_fd_(other.ep_fd_) {
  initializeServerAddr();
  instance_ = this;
}

// Copy assignment operator
IrcServ &IrcServ::operator=(const IrcServ &other) {
  if (this != &other) {
    delete message_handler_;
    channels_ = other.channels_;
    clients_to_close = other.clients_to_close;
    clients_ = other.clients_;
    password_ = other.password_;
    hostname_ = other.hostname_;
    port_ = other.port_;
    server_fd_ = other.server_fd_;
    ep_fd_ = other.ep_fd_;
    initializeServerAddr();
    message_handler_ = new MessageHandler(*this);
    instance_ = this;
  }
  return *this;
}

IrcServ::~IrcServ() {
}

void IrcServ::epoll_in_out(int client_fd) {
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT; // Listen for both input and output events
  ev.data.fd = client_fd;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_MOD, client_fd, &ev) == -1) {
    perror("Error modifying client socket to include EPOLLOUT");
    return;
  }
}

void IrcServ::epoll_in(int client_fd) {
  epoll_event ev;
  ev.events = EPOLLIN; // Listen for both input and output events
  ev.data.fd = client_fd;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_MOD, client_fd, &ev) == -1) {
    perror("Error modifying client socket to include EPOLLOUT");
    return;
  }
}

bool IrcServ::check_password(std::string& password) {
  if (password == password_) {
    return true;
  }
  return false;
}

string IrcServ::to_upper(const std::string& str) {
  std::string upper_str = str;
  std::transform(upper_str.begin(), upper_str.end(), upper_str.begin(), ::toupper);
  return upper_str;
}

bool IrcServ::check_nick(std::string& nick) {
  std::string upper_nick = to_upper(nick);
  std::map<int, Client*>::const_iterator it;
  for (it = clients_.begin(); it != clients_.end(); ++it) {
    if (to_upper(it->second->nick_) == upper_nick) {
      return false;
    }
  }
  return true;
}

void IrcServ::add_to_close(Client* client) {
  clients_to_close.insert(client);
}

// this is only used for clients that needed to receive a msg before they are getting closed
void IrcServ::cleanup_clients() {
  if (clients_to_close.empty()) {
    return;
  }
  std::set<Client*>::iterator it = clients_to_close.begin();
  for (; it != clients_to_close.end(); ++it) {
    delete_client((*it)->fd_);
  }
  clients_to_close.clear();
}

Channel* IrcServ::get_channel(const std::string& name) {
  std::string lower_name = to_upper(name);
  for (std::map<const std::string, Channel*>::const_iterator it = channels_.begin(); it != channels_.end(); ++it) {
    if (to_upper(it->first) == lower_name) {
      return it->second;
    }
  }
  return NULL;
}

Client* IrcServ::get_client(const std::string& name) {
  std::string lower_name = to_upper(name);
  for (std::map<int, Client*>::const_iterator it = clients_.begin(); it != clients_.end(); ++it) {
    if (to_upper(it->second->nick_) == lower_name) {
      return it->second;
    }
  }
  return NULL;
}

void IrcServ::create_channel(const std::string& name, Client& client) {
  Channel* channel = new Channel(*this, name, client);
  channels_[name] = channel;
  channel->add_client(client);
  channel->add_operator(client);
}

void IrcServ::signal_handler(int signal) {
  cout << "Signal " << signal << " received, cleaning up and exiting..." << endl;
  if (instance_) {
      instance_->cleanup();
  }
  exit(0);
}

void IrcServ::close_socket(int fd) {
  if (fd != static_cast<uint16_t>(-1)) {
    if (close(fd) == -1) {
      perror("Error closing socket");
    } else {
      std::cout << "Socket " << fd << " closed successfully." << std::endl;
    }
  }
}

void IrcServ::delete_client(int client_fd) {
  if (epoll_ctl(ep_fd_, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
    perror("Error removing client socket from epoll (delete client)");
  }
  map<string, Channel*>::iterator it = channels_.begin();
  for (; it != channels_.end(); ++it) {
    (*it).second->remove_client_invite_list(client_fd);
  }
  delete clients_[client_fd];
  clients_.erase(client_fd);
}

void IrcServ::cleanup() {
  // Close and delete all client connections
  std::vector<int> client_fds;
  for (std::map<int, Client*>::iterator it = clients_.begin(); it != clients_.end(); ++it) {
    client_fds.push_back(it->first);
  }
  for (std::vector<int>::iterator it = client_fds.begin(); it != client_fds.end(); ++it) {
    delete_client(*it);
  }
  clients_.clear();
  for (std::map<string, Channel*>::iterator it = channels_.begin(); it != channels_.end(); ++it) {
    delete it->second;
  }
  channels_.clear();
  delete message_handler_;
  close_socket(server_fd_);
  close_socket(ep_fd_);
}

void IrcServ::register_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = IrcServ::signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
      perror("Error registering SIGINT handler");
      exit(EXIT_FAILURE);
  }

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
      perror("Error registering SIGTERM handler");
      exit(EXIT_FAILURE);
  }
}

void IrcServ::set_non_block(int sock_fd) {
  int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("Error. Failed to get socket flags");
    exit(EXIT_FAILURE);
  }
  if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("Error. Failed to set socket to non-blocking mode");
    exit(EXIT_FAILURE);
  }
}

void IrcServ::initializeServerAddr() {
  memset(&server_addr_, 0, sizeof(server_addr_));
  server_addr_.sin_family = AF_INET;
  // server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); // Listen on localhost
  server_addr_.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
  server_addr_.sin_port = htons(static_cast<uint16_t>(port_));
}

bool IrcServ::add_fd_to_epoll(int fd) {
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    perror("Error adding server socket to epoll");
    return false;
  }
  return true;
}

void IrcServ::start() {
  register_signal_handlers();
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) { 
    perror("Error. Failed to create socket");
    exit(EXIT_FAILURE);
  }
  set_non_block(server_fd_);
  
  ep_fd_ = epoll_create1(0);
  if (ep_fd_ == -1) {
    perror("Error. Failed to create epoll instance");
    exit(EXIT_FAILURE);
  }

  if(!add_fd_to_epoll(server_fd_)) {
    perror("Error adding server socket to epoll");
    exit(EXIT_FAILURE);
  }

  if (bind(server_fd_, (sockaddr*)&server_addr_, sizeof(server_addr_)) == -1) {
    perror("Error. Failed binding on port");
    cleanup();
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd_, 10) == -1) {
    perror("Error. Failed to listen on socket");
    exit(EXIT_FAILURE);
  }

// Optional - Get the IP address on which the server is listening
  sockaddr_in bound_addr;
  socklen_t bound_addr_len = sizeof(bound_addr);
  if (getsockname(server_fd_, (sockaddr*)&bound_addr, &bound_addr_len) == -1) {
    perror("Error getting socket name");
    cleanup();
    exit(EXIT_FAILURE);
  }
  char* bound_ip = inet_ntoa(bound_addr.sin_addr);
  cout << "Server started on IP " << bound_ip << " and port " << port_ << endl;

  event_loop();
}

void IrcServ::event_loop() {
  while (true) {
    int client_fd = -1;
    epoll_event events[100];
    int n = epoll_wait(ep_fd_, events, 100, -1);
    if (n == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == server_fd_) { // event on server fd
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        client_fd = accept(server_fd_, (sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
          perror("Error accepting connection");
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No incoming connections, continue the loop
            continue;
          } else {
            exit(EXIT_FAILURE);
          }
        }
        set_non_block(client_fd);
        char* client_ip = inet_ntoa(client_addr.sin_addr);
        std::string client_hostname(client_ip);
        if (!add_fd_to_epoll(client_fd)) {
          perror("Error adding client socket to epoll");
          close(client_fd);
          continue;
        }
        Client* client = new (std::nothrow) Client(client_fd, client_addr, addr_len, client_hostname);
        if (client == NULL) {
          std::cerr << "Memory allocation failed for client" << std::endl;
          return;
        }
        clients_[client_fd] = client;
        if (password_.empty()) {
          client->state_ = WAITING_FOR_NICK;
        }
        else {
          client->state_ = WAITING_FOR_PASS;
        }
      }
      else if (events[i].events & EPOLLOUT) { // fd is ready to send messages
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        message_handler_->send_messages(*client);
        cleanup_clients();
      }
      else if (events[i].events & EPOLLIN) { // event on client fd (incoming message)
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        ssize_t bytes_read;
        char buffer[4096];
        if ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
          buffer[bytes_read] = '\0';
          client->add_buffer_to(buffer);
          cout << "\033[31m" << buffer << "\033[0m" << '\n'; // debug purposes       
          message_handler_->process_incoming_messages(*client);
        } 
        else if (bytes_read == 0 || (bytes_read == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
          // Handle client disconnection or error
          if (bytes_read == 0) {
              cout << "Client disconnected" << endl;
          } else {
              perror("Error. Failed to read from client");
          }
          delete_client(client_fd);
        }
      }
    }
  }
}
