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

	void connectServer(std::stringstream& ss, bool pw);

	bool handleCmd(const std::string& s);
	void handlePacket(sf::Packet& p);
	void updateList(sf::Packet& p);

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
	int fps = 10;

	//does the server know about your existance?
	bool isInitialized = false;
	//do you have admin privileges?
	bool isAdmin = false;

	bool isSshActive = false;
	bool isSendingMouse = false;
	bool isSendingKeyboard = false;

	sf::Vector2f lastMousePos;
	static Attacker* attacker;
};