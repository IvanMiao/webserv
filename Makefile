NAME	:= webserv
CC		:= c++
FLAG	:= -Wall -Wextra -Werror -std=c++98
INCLUDE	:= -I src -I src/server -I src/config -I src/utils -I src/router -I src/http -I src/cgi

SRC_FILES	:= main.cpp \
				config/ConfigParser.cpp \
				server/Server.cpp server/Client.cpp \
				http/HttpRequest.cpp http/HttpResponse.cpp \
				utils/Logger.cpp utils/StringUtils.cpp

SRC_DIR	:= src
SRC		:= $(addprefix $(SRC_DIR)/, $(SRC_FILES))

OBJ_DIR	:= obj
OBJ		:= $(addprefix $(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

TEST_PARSER		:= test_parser
TEST_PARSER_SRC	:= test/test_configparser.cpp \
				   src/config/ConfigParser.cpp \
				   src/utils/StringUtils.cpp

TEST_SERVER		:= test_server
TEST_SERVER_SRC	:= test/test_server.cpp \
				   src/config/ConfigParser.cpp \
				   src/server/Server.cpp \
				   src/server/Client.cpp \
				   src/http/HttpRequest.cpp \
				   src/http/HttpResponse.cpp \
				   src/utils/Logger.cpp \
				   src/utils/StringUtils.cpp

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(FLAG) $(INCLUDE) $(OBJ) -o $(NAME)

# ----- Test -----
check: $(TEST_PARSER) $(TEST_SERVER)
	@echo "----- Running parser tests... -----"
	./$(TEST_PARSER) test/test.conf
	@echo "\n----- Running server tests... -----"
	./$(TEST_SERVER)

test-server: $(TEST_SERVER)
	./$(TEST_SERVER)

$(TEST_PARSER): $(TEST_PARSER_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_PARSER_SRC) -o $(TEST_PARSER)

$(TEST_SERVER): $(TEST_SERVER_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_SERVER_SRC) -o $(TEST_SERVER)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(FLAG) $(INCLUDE) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME)
	rm -rf $(TEST_PARSER)
	rm -rf $(TEST_SERVER)

re: fclean all

.PHONY: all clean fclean re check test-server
