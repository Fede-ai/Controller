#include "server.h"

int main()
{	
	Server server;
	server.outputAddresses();

	reconnect:
	server.initConnection();
	if (server.control() == 3)
		goto reconnect;

	return 0;
}	