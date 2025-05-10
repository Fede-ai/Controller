#include <SFML/Network.hpp>
#include <iostream>

int main() {
	sf::UdpSocket server;
	if (server.bind(sf::Socket::AnyPort) == sf::Socket::Status::Done)
		std::cout << "binded udp socket to port " << server.getLocalPort() << "\n";
	else {
		std::cout << "failed to bind udp socket\n";
		return -1;
	}

	while (true) {
		std::string msg;
		std::cin >> msg;
		sf::Packet p;
		p << msg;

		server.send(p, sf::IpAddress::getLocalAddress().value(), 1600);
	}

	return 0;
}