#include <iostream>
#include <thread>
#include "tunnel.h"

int main()
{
	std::vector<sf::TcpSocket> v, c, clients;
	std::vector<Tunnel> tunnels;
	sf::TcpListener listener;

	if (listener.listen(53000) != sf::Socket::Done) {
		std::cout << "listening failed\n";
		return -1;
	}

	sf::SocketSelector selector;
	selector.add(listener);

	while (true) {
		if (!selector.wait())
			continue;
		bool processed = false;

		//check if a victims has received something
		for (int i = 0; i < v.size(); i++)
		{

		}
		//check if a controller has received something
		for (int i = 0; i < c.size(); i++)
		{

		}
		//check if a client is ready to init
		for (int i = 0; i < clients.size(); i++) {

		}
		//check if a client is ready to connect
		if (selector.isReady(listener)) {

		}
	}
	return 0;
}
