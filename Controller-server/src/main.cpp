#include "server.h"

int main()
{	
	Server server;
	server.outputAddresses();

	reconnect:
	server.initConnection();

	control:
	if (server.control() == 1)
		goto reconnect;
	else
		goto control;

	return 0;
}	