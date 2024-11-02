# CC = clang++
CC = g++
# CFLAGS = -Wall -Wextra -Werror -Weffc++ -Wconversion -Wsign-conversion -Wshadow -g -std=c++98 -Iincludes
CFLAGS = -Wall -Wextra -Wsign-conversion -Wshadow -g -std=c++98 -Iincludes
NAME = ircserv
SRC = $(wildcard srcs/*.cpp)
OBJ = $(SRC:.cpp=.o)
HEADER = $(wildcard includes/*.hpp)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ)

%.o: %.cpp $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all