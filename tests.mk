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
				   src/router/RequestHandler.cpp \
				   src/router/FileHandler.cpp \
				   src/router/UploadHandler.cpp \
				   src/router/ErrorHandler.cpp \
				   src/utils/Logger.cpp \
				   src/utils/StringUtils.cpp

TEST_HTTP_REQUEST		:= test_httprequest
TEST_HTTP_REQUEST_SRC	:= test/test_httprequest.cpp \
						   src/http/HttpRequest.cpp \
						   src/utils/StringUtils.cpp

TEST_HTTP_RESPONSE		:= test_httpresponse
TEST_HTTP_RESPONSE_SRC	:= test/test_httpresponse.cpp \
						   src/http/HttpResponse.cpp \
						   src/utils/StringUtils.cpp

TEST_REQUEST_HANDLER    := test_requesthandler
TEST_REQUEST_HANDLER_SRC := test/test_requesthandler.cpp \
                           src/config/ConfigParser.cpp \
                           src/http/HttpRequest.cpp \
                           src/http/HttpResponse.cpp \
                           src/router/RequestHandler.cpp \
                           src/router/FileHandler.cpp \
                           src/router/UploadHandler.cpp \
                           src/router/ErrorHandler.cpp \
                           src/utils/Logger.cpp \
                           src/utils/StringUtils.cpp

TEST_EXECUTABLES := $(TEST_PARSER) $(TEST_SERVER) $(TEST_HTTP_REQUEST) $(TEST_HTTP_RESPONSE) $(TEST_REQUEST_HANDLER)

# ----- Test Rules -----
check: $(TEST_EXECUTABLES)
	@echo "----- Running parser tests... -----"
	./$(TEST_PARSER) test/test.conf
	@echo "\n----- Running server tests... -----"
	./$(TEST_SERVER)
	@echo "\n----- Running HttpRequest tests... -----"
	./$(TEST_HTTP_REQUEST)
	@echo "\n----- Running HttpResponse tests... -----"
	./$(TEST_HTTP_RESPONSE)
	@echo "\n----- Running RequestHandler tests... -----"
	./$(TEST_REQUEST_HANDLER)

$(TEST_PARSER): $(TEST_PARSER_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_PARSER_SRC) -o $(TEST_PARSER)

$(TEST_SERVER): $(TEST_SERVER_SRC)
$(TEST_HTTP_REQUEST): $(TEST_HTTP_REQUEST_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_HTTP_REQUEST_SRC) -o $(TEST_HTTP_REQUEST)

$(TEST_REQUEST_HANDLER): $(TEST_REQUEST_HANDLER_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_REQUEST_HANDLER_SRC) -o $(TEST_REQUEST_HANDLER)
$(TEST_HTTP_REQUEST): $(TEST_HTTP_REQUEST_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_HTTP_REQUEST_SRC) -o $(TEST_HTTP_REQUEST)

$(TEST_HTTP_RESPONSE): $(TEST_HTTP_RESPONSE_SRC)
	$(CC) $(FLAG) $(INCLUDE) $(TEST_HTTP_RESPONSE_SRC) -o $(TEST_HTTP_RESPONSE)
