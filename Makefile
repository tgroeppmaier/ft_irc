CC = c++
CFLAGS = -Wall -Weffc++ -Wextra -Wconversion -Wsign-conversion -Werror -Wshadow -g -std=c++98
NAME = ircserv
SRC = main.cpp
OBJ = $(SRC:.cpp=.o)
HEADER = 

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ)

%.o: %.cpp $(HEADER)
	$(CC) $(CFLAGS) -c $<  -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all