#include "server/Server.hpp"
#include <iostream>

int	main(void)
{
	try
	{
		wsv::Server my_server(8080);
		my_server.start();
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return 1;
	}

	return 0;
}
