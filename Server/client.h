#pragma once
#include <SFML/Network.hpp>
#include "Mlib/System/system.hpp"

struct Client
{
public:
	sf::TcpSocket* socket = new sf::TcpSocket();
	sf::Uint32 time = Mlib::currentTime().asSec();
	char role = '-'; //can be '-', 'v', 'c'
	sf::Uint16 pair = 0; //the id of this client's pair
	size_t lastMsg = Mlib::currentTime().asMil();
	std::string name = "";
};