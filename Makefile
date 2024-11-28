# CC = clang++
CC = g++
# CFLAGS = -Wall -Wextra -Weffc++ -Wpedantic -Wconversion -Wsign-conversion -Wshadow -g -std=c++98 -Iincludes
CFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -g -std=c++98 -Iincludes
NAME = ircserv
SRC = srcs/Channel.cpp srcs/Client.cpp srcs/IrcServ.cpp srcs/main.cpp srcs/MessageHandler.cpp
OBJ = $(SRC:.cpp=.o)
HEADER = includes/Channel.hpp includes/Client.hpp includes/IrcServ.hpp includes/MessageHandler.hpp

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $(NAME) $(OBJ)

%.o: %.cpp $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re