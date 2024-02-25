#include "server.h"

int main()
{
	Server server;

	while (true)
		server.receive();

	return 0;
}
