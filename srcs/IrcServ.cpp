#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <cstdlib>
#include <pthread.h>
#include <signal.h>

#include "IrcServ.hpp"


using namespace std;

IrcServ* IrcServ::instance_ = NULL;

IrcServ::IrcServ(int port) : port_(port) {
  initializeServerAddr();
  instance_ = this;
    command_map_["CAP"] = &IrcServ::command_CAP;
    command_map_["NICK"] = &IrcServ::command_NICK;
    command_map_["USER"] = &IrcServ::command_USER;
}

IrcServ::IrcServ(int port, string password) : port_(port), password_(password) {
  initializeServerAddr();
  instance_ = this;
    command_map_["CAP"] = &IrcServ::command_CAP;
    command_map_["NICK"] = &IrcServ::command_NICK;
    command_map_["USER"] = &IrcServ::command_USER;
}


void IrcServ::signal_handler(int signal) {
  cout << "Signal " << signal << " received, cleaning up and exiting..." << endl;
  if (instance_) {
      instance_->cleanup();
  }
  exit(0);
}

void IrcServ::cleanup() {
  if (server_fd_ != -1) {
      close(server_fd_);
      server_fd_ = -1;
  }
  if (ep_fd_ != -1) {
      close(ep_fd_);
      ep_fd_ = -1;
  }
  cout << "######################KILLED BY SIGNAL#############################" << endl;

  // Close and delete all client connections
  for (std::map<int, Client*>::iterator it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->first != -1) {
        close(it->first);
    }
    delete it->second;
  }
  clients_.clear();
  command_map_.clear();
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

void IrcServ::close_client_fd(int client_fd) {
  if (epoll_ctl(ep_fd_, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
      perror("Error removing client socket from epoll");
  }
  if (close(client_fd) == -1) {
      perror("Error closing client socket");
  }
  delete clients_[client_fd];
  clients_.erase(client_fd);
}

void print_args(Client& client, vector<string>& arguments) {
  for(vector<string>::iterator it = arguments.begin(); it != arguments.end(); ++it) {
    cout << *it << ' ';
  }
  cout << endl;
}

void IrcServ::command_CAP(Client& client, vector<string>& arguments) {
  std::cout << "Handling CAP command for client " << client.fd_ << std::endl;
  print_args(client, arguments);
}

  void IrcServ::command_NICK(Client& client, vector<string>& arguments) {
  if (!arguments[0].empty()) {
    client.nick_ = arguments[0];
  }
  print_args(client, arguments);
  std::cout << "Handling NICK command for client " << client.fd_ << std::endl;
}

void IrcServ::send_message(Client& client, const char* message, size_t length) {
  // size_t length = message.size();
  size_t bytes_sent = send(client.fd_, message, length, 0);
  if ((bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
    UnsentMessage full_message = {client.fd_, length, message};
    unsent_messages_.push_back(full_message);
  }
}

void IrcServ::command_USER(Client& client, vector<string>& arguments) {
    std::cout << "Handling USER command for client " << client.fd_ << std::endl;
    print_args(client, arguments);

    if (arguments.size() < 4) {
        std::string response = "461 " + client.nick_ + " USER :Not enough parameters\r\n";
        send_message(client, response.c_str(), response.length());
        // send(client.fd_, response.c_str(), response.length(), 0);
        return;
    }

    client.username_ = arguments[0];
    client.hostname_ = arguments[1];
    client.servername_ = arguments[2];
    client.realname_ = arguments[3];

    string response = "001 " + client.nick_ + " :Welcome to the IRC server\r\n";
    send(client.fd_, response.c_str(), response.length(), 0);

    response = "002 " + client.nick_ + " :Your host is " + "server_addr_.sin_addr.s_addr "+ ", running version 1.0\r\n";
    send(client.fd_, response.c_str(), response.length(), 0);

    response = "003 " + client.nick_ + " :This server was created today\r\n";
    send(client.fd_, response.c_str(), response.length(), 0);

    response = "004 " + client.nick_ + " " + "server_addr_.sin_addr.s_addr" + " 1.0 o o\r\n";
    send(client.fd_, response.c_str(), response.length(), 0);
}



void IrcServ::set_non_block(int sock_fd) {
  int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1) {
    cerr << "Error. Failed to get socket flags: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    cerr << "Error. Failed to set socket to non-blocking mode: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
}

void IrcServ::initializeServerAddr() {
  memset(&server_addr_, 0, sizeof(server_addr_));
  server_addr_.sin_family = AF_INET;
  server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); // INADDR_ANY; Listen on all interfaces
  server_addr_.sin_port = htons(port_);
}

void IrcServ::start() {
  register_signal_handlers();
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) {
    cerr << "Error. Failed to create socket" << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  set_non_block(server_fd_);
  
  // create epoll instance
  ep_fd_ = epoll_create1(0);
  if (ep_fd_ == -1) {
    perror("Error. Failed to create epoll instance");
    exit(EXIT_FAILURE);
  }

  // add server socket to epoll
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = server_fd_;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, server_fd_, &ev) == -1) {
    perror("Error adding server socket to epoll");
    exit(EXIT_FAILURE);
  }

  if (bind(server_fd_, (sockaddr*)&server_addr_, sizeof(server_addr_)) == -1) {
    cerr << "Error. Failed to bind to port " << port_ << " " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd_, 10) == -1) {
    cerr << "Error. Failed to listen on socket " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Server started on port " << port_ << endl;
  event_loop();
}

void IrcServ::event_loop() {
  MessageHandler message_handler(*this);
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
        epoll_event client_ev;
        client_ev.events = EPOLLIN; // Use level-triggered mode (default)
        client_ev.data.fd = client_fd;
        if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
          perror("Error adding client socket to epoll");
          close(client_fd);
          continue;
        }
        clients_[client_fd] = new Client(client_fd, client_addr, addr_len);
      } 
      else { // event on client fd
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        int bytes_read;
        char buffer[513];
        bool new_data_added = false; 
        while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
          buffer[bytes_read] = '\0';
          client->add_buffer_to(buffer);
          new_data_added = true; 
        }
        if (new_data_added) {
          message_handler.process_incoming_messages(*client);
          // for (vector<string>::iterator it = client->messages_.begin(); it != client->messages_.end(); ++it) {
          //   cout << *it << endl;
          // }
        }
        if (bytes_read == 0 || (bytes_read == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
          if (bytes_read == 0) {
              cout << "Client disconnected" << endl;
          } else {
              perror("Error. Failed to read from client");
          }
          close_client_fd(client_fd);
        }
      }
    }
  }
}