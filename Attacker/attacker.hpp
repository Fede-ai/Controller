#include <SFML/Network.hpp>
#include <SFML/System.hpp>
#include <thread>
#include <iostream>

class Attacker {
public:
	Attacker(std::string inHId);
	int runConsoleShell();

private:
	void receiveTcp();

	void connectServer(std::stringstream& ss, bool pw);
	void updateList(sf::Packet& p);

	inline void printErr(std::string e) {
		std::cerr << "\033[31m" << e << "\033[0m";
		consoleMemory << "\033[31m" << e << "\033[0m";
	}
	inline void printMsg(std::string s) {
		std::cout << s;
		consoleMemory << s;
	}

	sf::TcpSocket server;
	sf::IpAddress serverIp = sf::IpAddress(0, 0, 0, 0);

	//uint8_t
	enum Cmd {
		UNDEFINED = 0,
		REGISTER_ADMIN = 1,
		REGISTER_ATTACKER = 2,
		REGISTER_VICTIM = 3,
		LIST_UPDATE = 4,
		CHANGE_NAME = 5,	//admin
		BAN_HID = 6,		//admin
		UNBAN_HID = 7,		//admin
		KILL = 8,			//admin
	};
	std::map<uint16_t, sf::Packet> pendingResponses;
	uint16_t requestId = 1;

	std::string myHId = "";
	int16_t myId = 0;

	bool needRefreshConsole = false;
	std::thread* receiveThread = nullptr;
	std::stringstream consoleMemory, listMemory;

	//does the server know about your existance?
	bool isInitialized = false;
	//do you have admin privileges
	bool isAdmin = false;
};