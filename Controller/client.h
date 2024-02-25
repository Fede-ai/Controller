#pragma once
#include <string>
#include "SFML/System.hpp"

struct Client
{
	Client(sf::Uint16 inId, sf::Uint32 inTime, std::string inIp, sf::Uint16 inPort, sf::Uint16 inOtherId)
		:
		id(inId),
		time(inTime),
		ip(inIp),
		port(inPort),
		otherId(inOtherId)
	{}

	sf::Uint16 id = 0, otherId = 0;
	sf::Uint32 time = 0;
	std::string ip = "";
	sf::Uint16 port = 0;
};