#pragma once
#include "SFML/Network.hpp"
#include "SFML/Graphics.hpp"
#include <iostream>
#include <chrono>
#include <windows.h>

class Server
{
public:	
	void outputAddresses();
	void initConnection();
	short control();

private:
	void fillPacket(sf::Packet& packet);
	static void updateKeys(short* keys);

	sf::RenderWindow window;
	sf::TcpListener listener;
	sf::TcpSocket client;
	
	short lastKeys[256];
	int currentFps = 0;
	sf::Int8 deltaWheel = 0;
	sf::Int16 lastXProp; //0 -> 10000
	sf::Int16 lastYProp; //0 -> 10000
};

