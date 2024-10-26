#include <iostream>
#include <string>
#include <sstream>
#include "IrcServ.hpp"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        cerr << "Usage: " << argv[0] << " <port> <password>" << endl;
        return 1;
    }

    // Extract and validate the port number
    string port_str = argv[1];
    int port;
    stringstream ss(port_str);
    if (!(ss >> port) || !ss.eof()) {
        cerr << "Invalid port number: " << port_str << endl;
        return 1;
    }

    // Extract the password
    // string password = argv[2];

    // Create and start the IRC server
    IrcServ server(port);
    server.start();

    return 0;
}