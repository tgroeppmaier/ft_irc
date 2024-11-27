#!/bin/bash

# Configuration
SERVER="127.0.0.1" # Replace with your IRC server's IP or hostname
PORT="6667"        # Replace with your IRC server's port
# PASS="test"
NICK="testi"    # Nickname to use
USER="testuser 0 * :TestUser" # USER command
CHANNEL="#fuu"     # Channel to join

# Connect to the server using netcat
{
    # echo "PASS $PASS"
    echo "NICK $NICK"
    echo "USER $USER"
    sleep 1 # Allow time for registration

    # Join the channel
    echo "JOIN $CHANNEL"
    sleep 1 # Allow time for the JOIN command to be processed

    # Spam incrementing numbers
    COUNT=1
    while true; do
        echo "PRIVMSG $CHANNEL :$COUNT"
        ((COUNT++))
        sleep .01
    done
} | telnet $SERVER $PORT
