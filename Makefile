# --- Variables ---
NAME        := webserv
CC          := c++
FLAG          = -Wall -Wextra -Werror -std=c++98 -g
LIBS        := -lstdc++ -lrt # Needed for POSIX features (epoll, socket, etc.)

# --- Directory Setup ---
SRC_DIR     := src
OBJ_DIR     := obj
TEST_DIR    := test

# --- Server Source Files ---
# List all core .cpp files relative to SRC_DIR (excluding main.cpp for tests)
SERVER_SRC_FILES := cgi/CgiHandler.cpp \
                    config/ConfigParser.cpp \
                    http/HttpRequest.cpp \
                    http/HttpResponse.cpp \
                    request/CgiRequestHandler.cpp \
                    request/ErrorHandler.cpp \
                    request/FileHandler.cpp \
                    request/RequestHandler.cpp \
                    request/UploadHandler.cpp \
                    server/Server.cpp \
                    server/Client.cpp \
                    utils/Logger.cpp \
                    utils/StringHelper.cpp

# Main server executable needs main.cpp
SERVER_SRC_WITH_MAIN := main.cpp $(SERVER_SRC_FILES)

SERVER_SRC  := $(addprefix $(SRC_DIR)/, $(SERVER_SRC_FILES))
SERVER_OBJ  := $(addprefix $(OBJ_DIR)/, $(SERVER_SRC_FILES:%.cpp=%.o))

# --- Test Variables ---
TEST_NAME   := webserv_tests
# All test files + all core server files (but NOT main.cpp)
TEST_SRC_FILES := $(TEST_DIR)/test_main.cpp \
                  $(TEST_DIR)/CgiHandlerTest.cpp \
                  $(TEST_DIR)/test_parser.cpp \
                  $(addprefix $(SRC_DIR)/, $(SERVER_SRC_FILES))

# --- Header Inclusion Paths (-I flags) ---
# This is crucial for compilation
INCLUDES    := -I $(SRC_DIR) \
               -I $(SRC_DIR)/cgi \
               -I $(SRC_DIR)/config \
               -I $(SRC_DIR)/http \
               -I $(SRC_DIR)/server \
               -I $(SRC_DIR)/request \
               -I $(SRC_DIR)/utils \
               -I $(TEST_DIR)

# --- Targets ---

all: $(NAME)

# Main Server Executable (Final Link)
$(NAME): $(OBJ_DIR)/main.o $(SERVER_OBJ)
	$(CC) $(FLAG) $(OBJ_DIR)/main.o $(SERVER_OBJ) $(LIBS) -o $(NAME)

# Rule to compile object files
# Uses the comprehensive INCLUDES list
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(FLAG) $(INCLUDES) -c $< -o $@

# ----------------------------------------------------
# Testing Targets
# ----------------------------------------------------

# Full Test Runner: Compiles all tests and server source, then runs the executable
test: $(TEST_NAME)
	./$(TEST_NAME)

# Test Executable (Linking tests and all server logic)
$(TEST_NAME): $(SERVER_OBJ) $(TEST_SRC_FILES)
	$(CC) $(FLAG) $(INCLUDES) $(TEST_SRC_FILES) $(LIBS) -o $(TEST_NAME)

# Legacy Config Parser Test (Retained your original 'check' target logic)
check: test_parser
	./test_parser test/test.conf

test_parser: $(TEST_DIR)/test_parser.cpp $(SRC_DIR)/config/ConfigParser.cpp
	$(CC) $(FLAG) -I $(SRC_DIR)/config -I $(TEST_DIR) $^ -o $@

# ----------------------------------------------------
# Clean Targets
# ----------------------------------------------------

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)
	rm -f $(TEST_NAME)
	rm -f test_parser

re: fclean all

.PHONY: all clean fclean re test check
