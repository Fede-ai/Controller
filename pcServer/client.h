#pragma once
#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"

struct Client
{
public:
	Client(int inId) : id(inId) {};

	sf::TcpSocket* socket = new sf::TcpSocket();
	sf::Uint32 time = Mlib::getTime()/1000;
	sf::Uint16 id;
	char role = '-'; //can be '-', 'v', 'c'
	Client* pair = nullptr;
};