NAME	:= webserv
CC		:= c++
FLAG	:= -Wall -Wextra -Werror -std=c++98

SRC_FILES	:= main.cpp \
				server/Server.cpp

SRC_DIR	:= src
SRC		:= $(addprefix $(SRC_DIR)/, $(SRC_FILES))

OBJ_DIR	:= obj
OBJ		:= $(addprefix $(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(FLAG) $(OBJ) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(FLAG) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME)

re: fclean all

.PHONY: all clean fclean re
