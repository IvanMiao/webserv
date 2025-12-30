NAME	:= webserv
CC		:= c++
FLAG	:= -Wall -Wextra -Werror -std=c++98
INCLUDE	:= -I src -I src/server -I src/config -I src/utils -I src/router -I src/http -I src/cgi

SRC_FILES	:= main.cpp \
				config/ConfigParser.cpp \
				server/Server.cpp server/Client.cpp \
				http/HttpRequest.cpp http/HttpResponse.cpp \
				router/RequestHandler.cpp router/FileHandler.cpp \
				router/ErrorHandler.cpp router/UploadHandler.cpp \
				utils/Logger.cpp utils/StringUtils.cpp

SRC_DIR	:= src
SRC		:= $(addprefix $(SRC_DIR)/, $(SRC_FILES))

OBJ_DIR	:= obj
OBJ		:= $(addprefix $(OBJ_DIR)/,$(SRC_FILES:%.cpp=%.o))

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(FLAG) $(INCLUDE) $(OBJ) -o $(NAME)

include tests.mk

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(FLAG) $(INCLUDE) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME)
	rm -rf $(TEST_EXECUTABLES)

re: fclean all

.PHONY: all clean fclean re check
