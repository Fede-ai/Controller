#pragma once
#include <SFML/Network.hpp>

struct Client
{
public:
	sf::TcpSocket* socket = new sf::TcpSocket();
	char role = '-'; //can be '-', 'v', 'c'
	sf::Uint16 pair = 0; //the id of this client's pair
	//time of the last message received from this client (for afk detencion)
	size_t lastMsg = std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::system_clock::now().time_since_epoch()).count();
	std::string name = "new client", hardwareId = "";
	bool isAdmin = false;
};