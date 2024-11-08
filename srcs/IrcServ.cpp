#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <cstdlib>
// #include <pthread.h>
#include <signal.h>

#include "IrcServ.hpp"

// using namespaceusing std string;
using std::string;
using std::cout;
using std::cerr;
using std::endl;


IrcServ* IrcServ::instance_ = NULL;

IrcServ::IrcServ(int port) : 
  port_(port),
  server_fd_(0),
  ep_fd_(0),
  server_addr_(),
  clients_(),
  message_handler_(new MessageHandler(*this)) {
  initializeServerAddr();
  instance_ = this;
}

IrcServ::IrcServ(int port, string password) :
  port_(port), 
  server_fd_(0),
  password_(password),
  ep_fd_(0),
  server_addr_(),
  clients_(),
  message_handler_(new MessageHandler(*this)) {
  initializeServerAddr();
  instance_ = this;
}

IrcServ::~IrcServ() {
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
      std::cerr << "Error closing socket " << fd << ": " << std::strerror(errno) << std::endl;
    } else {
      std::cout << "Socket " << fd << " closed successfully." << std::endl;
    }
  }
}

void IrcServ::close_client_fd(int client_fd) {
  if (epoll_ctl(ep_fd_, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
      perror("Error removing client socket from epoll");
  }
  delete clients_[client_fd];
  clients_.erase(client_fd);
}

void IrcServ::cleanup() {
  cout << "######################KILLED BY SIGNAL#############################" << endl;
  close_socket(server_fd_);
  close_socket(ep_fd_);

  // Close and delete all client connections
  for (std::map<int, Client*>::iterator it = clients_.begin(); it != clients_.end(); ++it) {
    close_client_fd(it->first);
    delete it->second;
  }
  clients_.clear();
  delete message_handler_;
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
  server_addr_.sin_port = htons(static_cast<uint16_t>(port_));
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
      else if (events[i].events & EPOLLOUT) { // client fd is ready again
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        ssize_t bytes_sent;
        while (!client->unsent_messages_.empty()) {
          string message = client->unsent_messages_.front();
          bytes_sent = send(client->fd_, message.c_str(), message.length(), 0);
          if (bytes_sent == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;;
          }
          cout << "unsent message sent: " << message << endl;
          client->unsent_messages_.pop_front();
        }
        epoll_event ev;
        ev.events = EPOLLIN; // Listen for input events
        ev.data.fd = client->fd_;
        if (epoll_ctl(ep_fd_, EPOLL_CTL_MOD, client->fd_, &ev) == -1) {
          perror("Error modifying client socket to EPOLLIN");
        }
      }
      else { // event on client fd (incoming message)
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        ssize_t bytes_read;
        char buffer[513];
        // bool new_data_added = false; 
        if ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
          buffer[bytes_read] = '\0';
          client->add_buffer_to(buffer);
          cout << buffer << endl; // debug purposes
          message_handler_->process_incoming_messages(*client);
        } 
        else if (bytes_read == 0 || (bytes_read == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
          // Handle client disconnection or error
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
//         while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
//           buffer[bytes_read] = '\0';
//           client->add_buffer_to(buffer);
//           cout << buffer << endl; // for debug purposes
//           new_data_added = true; 
//         }
//         if (new_data_added) {
//           message_handler_->process_incoming_messages(*client);
//           // for (vector<string>::iterator it = client->messages_.begin(); it != client->messages_.end(); ++it) {
//           //   cout << *it << endl;
//           // }
//         }
//         if (bytes_read == 0 || (bytes_read == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
//           if (bytes_read == 0) {
//               cout << "Client disconnected" << endl;
//           } else {
//               perror("Error. Failed to read from client");
//           }
//           close_client_fd(client_fd);
//         }
//       }
//     }
//   }
// }