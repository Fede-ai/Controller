#pragma once
#include <iostream>
#include "client.h"

#define PORT 9002

class Server
{
public:
	Server();
	void receive();
	
private:
	void processControllerMsg(int i, sf::Packet p);
	void processVictimMsg(int i, sf::Packet p);
	void disconnect(int i);
	void updateControllersList();

	sf::Uint16 currentId = 0;
	sf::SocketSelector selector;
	sf::TcpListener listener;
	std::vector<Client> clients;
};

