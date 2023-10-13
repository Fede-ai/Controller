#pragma once
#include "SFML/Network.hpp"
#include "SFML/Graphics.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <windows.h>

class Server
{
public:	
	void run();

private:	
	void outputAddresses();
	void initConnection();
	//1 = disconnected
	void remoteControl();

	void fillPacket(sf::Packet& packet);
	void selectAndSendFile();

	static void updateKeys(short* keys);
	static size_t getTime();

	sf::RenderWindow window;
	sf::TcpListener listener;
	sf::TcpSocket client;

	bool isMouseEnabled = false;
	bool canEnableMouse = false;
	bool canLoadFile = false;

	bool isConnected = false;
	bool canDisconnect = false;
	size_t lastCheck = getTime();
	short lastKeys[256];
	int currentFps = 0;
	sf::Int16 lastXProp; //0 -> 10000
	sf::Int16 lastYProp; //0 -> 10000
};

