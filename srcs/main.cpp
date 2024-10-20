#include <string>
#include <sys/types.h>

#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <cstdlib> // For exit() and EXIT_FAILURE
#include <iostream> // For cout

#include <fcntl.h>         // For non-blocking mode
#include <unistd.h>        // For close()
#include <cstring>         // For memset()
#include <arpa/inet.h>     // For htons()
#include <cerrno>
#include <csignal>

#include "IrcServ.hpp"

// create socket
// bind socket
// listen
// accept connection
// read

int main (int argc, char *argv[]) {
  IrcServ server1(6697);
  server1.start();
  return(0);
}