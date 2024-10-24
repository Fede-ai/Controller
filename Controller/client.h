#pragma once
#include <string>
#include "SFML/System.hpp"

struct Client
{
	sf::Uint16 id = 0, otherId = 0;
	std::string ip = "";
	sf::Uint16 port = 0;
	std::string name = "", hardwareId;
	bool isAdmin = false;
};