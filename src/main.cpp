#include "server/Server.hpp"
#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"

#include <iostream>

int	main( int argc, char** argv )
{
	std::string config_path = "config/default.conf";
	if (argc > 1)
		config_path = argv[1];

	try
	{
		wsv::ConfigParser config(config_path);
		config.parse();

		wsv::Server my_server(config);
		my_server.start();
	}
	catch( const std::exception& e )
	{
		wsv::Logger::error(e.what());
		return 1;
	}

	return 0;
}
