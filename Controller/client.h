#pragma once
#include <string>
#include "SFML/System.hpp"

struct Client
{
	sf::Uint16 id = 0, otherId = 0;
	sf::Uint32 time = 0;
	std::string ip = "";
	sf::Uint16 port = 0;
	std::string name = "";
	bool isAdmin = false;
};