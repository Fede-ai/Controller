#pragma once
#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"

struct Client
{
public:
	sf::TcpSocket* socket = new sf::TcpSocket();
	sf::Uint32 time = Mlib::getTime()/1000;
	char role = '-'; //can be '-', 'v', 'c'
	sf::Uint16 pair = 0; //the id of this client's pair
};