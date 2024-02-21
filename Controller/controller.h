#pragma once
#include "client.h"
#include <SFML/Network.hpp>

#define SERVER_IP "2.235.241.210"
#define SERVER_PORT 2390

class Controller
{
public:
	Controller(sf::UdpSocket& s);
	void receiveInfo();

private:
	std::vector<Client> controllers, victims;
	bool isPaired = false, isControlling = false;
	sf::UdpSocket& socket;
};

