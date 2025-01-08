#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <climits>
#include "IrcServ.hpp"

static void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " <port> [password]\n"
            << "  port     - Port number (1024-65535)\n"
            << "  password - Optional server password\n";
}

static bool validate_port(const char* port_str, int& port) {
  if (!port_str && !*port_str) {
    std::cerr << "Error: Port cannot be empty\n";
    return false;
  }
  long tmp;
  std::stringstream ss(port_str);
  if (!(ss >> tmp) || !ss.eof() || ss.fail()) {
    std::cerr << "Error: Invalid port number or number too large\n";
    return false;
  }
  if (tmp <= 1024 || tmp > 65535) {
    std::cerr << "Error: Port must be between 1024 and 65535\n";
    return false;
  }
  port = static_cast<int>(tmp);
  return true;
}

static bool validate_password(const char* password) {
  if (!password || !*password) {
    std::cerr << "Error: Password cannot be empty\n";
    return false;
  }
  size_t len = std::strlen(password);
  if (len > 32) {
    std::cerr << "Error: Password too long (max 32 characters)\n";
    return false;
  }
  return true;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }
  int port;
  if (!validate_port(argv[1], port)) {
    return EXIT_FAILURE;
  }
  if (argc == 3) {
    if (!validate_password(argv[2])) {
      return EXIT_FAILURE;
    }
    IrcServ::getInstance(port, argv[2]).start();
  } 
  else {
    IrcServ::getInstance(port).start();
  }
  return EXIT_SUCCESS;
}