#include "server.hpp"

int main() {
	Server server;

	int status = 0;
	while (status == 0)
		status = server.processIncoming();

	return status;
}