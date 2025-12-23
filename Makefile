NAME	:= webserv
CC		:= c++
FLAG	:= -Wall -Wextra -Werror -std=c++98

SRC_FILES	:= main.cpp \
				config/ConfigParser.cpp \
				server/Server.cpp server/Client.cpp \
				utils/Logger.cpp utils/StringUtils.cpp

SRC_DIR	:= src
SRC		:= $(addprefix $(SRC_DIR)/, $(SRC_FILES))

OBJ_DIR	:= obj
OBJ		:= $(addprefix $(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

TEST_PARSER		:= test_parser
TEST_PARSER_SRC	:= test/test_parser.cpp src/config/ConfigParser.cpp src/utils/StringUtils.cpp

TEST_SERVER		:= test_server
TEST_SERVER_SRC	:= test/test_server.cpp \
				   src/config/ConfigParser.cpp \
				   src/server/Server.cpp \
				   src/server/Client.cpp \
				   src/utils/Logger.cpp \
				   src/utils/StringUtils.cpp

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(FLAG) $(OBJ) -o $(NAME)

# ----- Test -----
check: $(TEST_PARSER) $(TEST_SERVER)
	@echo "Running parser tests..."
	./$(TEST_PARSER) test/test.conf
	@echo ""
	@echo "Running server tests..."
	./$(TEST_SERVER)

test: $(TEST_SERVER)
	./$(TEST_SERVER)

test-all: check

$(TEST_PARSER): $(TEST_PARSER_SRC)
	$(CC) $(FLAG) -I src/config $(TEST_PARSER_SRC) -o $(TEST_PARSER)

$(TEST_SERVER): $(TEST_SERVER_SRC)
	$(CC) $(FLAG) -I src $(TEST_SERVER_SRC) -o $(TEST_SERVER)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(FLAG) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME)
	rm -rf $(TEST_PARSER)
	rm -rf $(TEST_SERVER)

re: fclean all

.PHONY: all clean fclean re check test test-all
