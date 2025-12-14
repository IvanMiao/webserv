NAME	:= webserv
CC		:= c++
FLAG	:= -Wall -Wextra -Werror -std=c++98

SRC_FILES	:= main.cpp \
				config/ConfigParser.cpp \
				server/Server.cpp server/Client.cpp \
				utils/Logger.cpp

SRC_DIR	:= src
SRC		:= $(addprefix $(SRC_DIR)/, $(SRC_FILES))

OBJ_DIR	:= obj
OBJ		:= $(addprefix $(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

TEST_NAME	:= test_parser
TEST_SRC	:= test/test_parser.cpp src/config/ConfigParser.cpp

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(FLAG) $(OBJ) -o $(NAME)

# ----- Test -----
check: $(TEST_NAME)
	./$(TEST_NAME) test/test.conf

$(TEST_NAME): $(TEST_SRC)
	$(CC) $(FLAG) -I src/config $(TEST_SRC) -o $(TEST_NAME)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(FLAG) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME)
	rm -rf $(TEST_NAME)

re: fclean all

.PHONY: all clean fclean re check
