#include <SFML/Network.hpp>
#include <iostream>

int main() {
	sf::UdpSocket udp;
	if (udp.bind(1600) == sf::Socket::Status::Done)
		std::cout << "binded udp socket to port 1600\n";
	else {
		std::cout << "failed to bind udp socket to port 1600\n";
		return -1;
	}

	while (true) {
		sf::Packet p;
		std::optional<sf::IpAddress> ip;
		unsigned short port;

		if (udp.receive(p, ip, port) == sf::Socket::Status::Done && ip.has_value()) {
			std::string msg;
			p >> msg;
			std::cout << ip.value() << ":" << port << " - " << msg << "\n";
		}
	}

	return 0;
}