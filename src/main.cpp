#include "server/Server.hpp"
#include "utils/Logger.hpp"

#include <iostream>

int	main( void )
{
	try
	{
		wsv::Server my_server(8080);
		my_server.start();
	}
	catch( const std::exception& e )
	{
		wsv::Logger::error(e.what());
		return 1;
	}

	return 0;
}
