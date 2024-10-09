#pragma once
#include <SFML/Network.hpp>

struct Client
{
public:
	sf::TcpSocket* socket = new sf::TcpSocket();
	sf::Uint32 time = sf::Uint32(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	char role = '-'; //can be '-', 'v', 'c'
	sf::Uint16 pair = 0; //the id of this client's pair
	size_t lastMsg = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	std::string name = "";
	bool isAdmin = false;
};