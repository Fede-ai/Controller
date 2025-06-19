#include "server.hpp"
#include <iostream>
#include <exception>
#include <set>

Server::Server()
{
	if (listener.listen(443) != sf::Socket::Status::Done)
		std::exit(-1);
	selector.add(listener);
	
	try {
		const auto& pub = sf::IpAddress::getPublicAddress();
		const auto& priv = sf::IpAddress::getLocalAddress();
		const auto& port = listener.getLocalPort();
		std::cout << getTime() << " - listening on port " << port << " (" << 
			priv.value().toString() << " / " << pub.value().toString() << ")\n";
	}
	catch (std::exception& e) {
		std::cout << e.what() << "\n";
		std::exit(-2);
	}
}

int Server::processIncoming()
{
	if (!selector.wait()) {
		sf::sleep(sf::milliseconds(1));
		return 0;
	}

	//listen for incoming connections
	if (selector.isReady(listener)) {
		Client c;
		c.id = nextId++;
		c.socket = new sf::TcpSocket();
		//client accepted successfully
		if (listener.accept(*c.socket) == sf::Socket::Status::Done) {
			uninitialized.push_back(c);
			selector.add(*c.socket);

			std::cout << getTime() << " - new connection (" << c.id << ") = " <<
				c.socket->getRemoteAddress().value() << ":" << c.socket->getRemotePort() << "\n";
		}
		//failed to accept client 
		else {
			delete c.socket;
			nextId--;
		}
	}

	//listen for initialization attempts
	std::set<std::uint16_t> initializedIds;
	for (int i = 0; i < uninitialized.size(); i++) {
		auto& u = uninitialized[i];
		if (!selector.isReady(*u.socket))
			continue;

		sf::Packet p;
		auto status = u.socket->receive(p);
		//forget disconnected client
		if (status == sf::Socket::Status::Disconnected) {
			std::cout << getTime() << " - uninitialized client disconnected (" << u.id << ")\n";

			selector.remove(*u.socket);
			delete u.socket;

			uninitialized.erase(uninitialized.begin() + i);
			i--;
			continue;
		}
		else if (status != sf::Socket::Status::Done)
			continue;

		std::uint16_t reqId;
		std::uint8_t cmd;
		p >> reqId >> cmd;

		//accept admin
		if (cmd == std::uint8_t(1)) {
			std::string hId, pw;
			p >> hId >> pw;
			if (database.find(hId) == database.end())
				database[hId] = HIdInfo();

			//password is correct
			if (pw == adminPass) {
				sf::Packet res;
				res << reqId << bool(true) << uint16_t(u.id);
				auto _ = u.socket->send(res);

				u.isAttacker = true, u.isAdmin = true, u.hId = hId;
				initializedIds.insert(u.id);
				clients.push_back(u);
				uninitialized.erase(uninitialized.begin() + i);

				std::cout << getTime() << " - " << u.id << " = admin: " << hId << "\n";
				sendClientList();
				i--;
			}
			//password is not correct
			else {
				sf::Packet res;
				res << reqId << bool(false);
				auto _ = u.socket->send(res);

				selector.remove(*u.socket);
				delete u.socket;

				std::cout << getTime() << " - failed admin login (" << u.id << ")\n";
				uninitialized.erase(uninitialized.begin() + i);
				i--;
			}
		}
		//accept attacker
		else if (cmd == std::uint8_t(2)) {
			std::string hId;
			p >> hId;
			if (database.find(hId) == database.end())
				database[hId] = HIdInfo();

			//client is banned (access denied)
			if (database[hId].isBanned) {
				sf::Packet res;
				res << reqId << bool(false);
				auto _ = u.socket->send(res);

				selector.remove(*u.socket);
				delete u.socket;

				std::cout << getTime() << " - banned client (" << u.id << "): " << hId << "\n";
				uninitialized.erase(uninitialized.begin() + i);
				i--;
			}
			//access granted
			else {
				sf::Packet res;
				res << reqId << bool(true) << uint16_t(u.id);
				auto _ = u.socket->send(res);

				u.isAttacker = true, u.hId = hId;
				initializedIds.insert(u.id);
				clients.push_back(u);
				uninitialized.erase(uninitialized.begin() + i);

				std::cout << getTime() << " - " << u.id << " = attacker: " << hId << "\n";
				sendClientList();
				i--;
			}
		}
		//accept victim
		else if (cmd == std::uint8_t(3)) {

			sendClientList();
		}
	}

	//listen for client communication
	std::set<std::string> bannedHIds;
	std::set<uint16_t> idsToKill;
	for (int i = 0; i < clients.size(); i++) {
		auto& c = clients[i];
		//skip just initialized clients
		if (initializedIds.find(c.id) != initializedIds.end())
			continue;
		if (!selector.isReady(*c.socket))
			continue;

		sf::Packet p;
		auto status = c.socket->receive(p);
		//forget disconnected client
		if (status == sf::Socket::Status::Disconnected) {
			std::cout << getTime() << " - client disconnected (" << c.id << ")\n";

			selector.remove(*c.socket);
			delete c.socket;

			clients.erase(clients.begin() + i);
			i--;

			sendClientList();
			continue;
		}
		else if (status != sf::Socket::Status::Done)
			continue;

		uint16_t reqId;
		uint8_t cmd;
		p >> reqId >> cmd;

		//change client name
		if (cmd == uint8_t(5)) {
			std::string hId, name;
			p >> hId >> name;
			if (c.isAdmin && database.find(hId) != database.end()) {
				database[hId].name = name;
				sendClientList();
			}
		}
		//ban client
		else if (cmd == uint8_t(6)) {
			std::string hId;
			p >> hId;

			if (c.isAdmin && database.find(hId) != database.end()) {
				if (!database[hId].isBanned) {
					database[hId].isBanned = true;
					bannedHIds.insert(hId);
					sendClientList();
				}
			}
		}
		//unban client
		else if (cmd == uint8_t(7)) {
			std::string hId;
			p >> hId;

			if (c.isAdmin && database.find(hId) != database.end()) {
				if (database[hId].isBanned) {
					database[hId].isBanned = false;
					sendClientList();
				}
			}
		}
		//kill client
		else if (cmd == uint8_t(8)) {
			uint16_t id;
			p >> id;

			if (c.isAdmin)
				idsToKill.insert(id);
		}
	}

	bool listUpdateNeeded = false;
	//kill all the banned clients
	for (const auto& hId : bannedHIds) {
		for (int i = 0; i < clients.size(); i++) {
			//check if client needs to be killed
			if (clients[i].hId == hId && clients[i].isAttacker && !clients[i].isAdmin) {
				std::cout << getTime() << " - client banned (" << clients[i].id << ")\n";

				selector.remove(*clients[i].socket);
				delete clients[i].socket;

				clients.erase(clients.begin() + i);
				listUpdateNeeded = true;
				i--;
			}
		}
	}
	//kill all the clients that need to
	for (const auto& id : idsToKill) {
		for (int i = 0; i < clients.size(); i++) {
			//check if client needs to be killed
			if (clients[i].id == id && !clients[i].isAdmin) {
				std::cout << getTime() << " - client killed (" << clients[i].id << ")\n";

				selector.remove(*clients[i].socket);
				delete clients[i].socket;

				clients.erase(clients.begin() + i);
				listUpdateNeeded = true;
				break;
			}
		}
	}
	if (listUpdateNeeded)
		sendClientList();

	return 0;
}

void Server::sendClientList()
{
	sf::Packet admPacket, attPacket;
	admPacket << uint16_t(0) << uint8_t(4) << uint16_t(database.size());
	attPacket << uint16_t(0) << uint8_t(4) << uint16_t(0);

	for (const auto& [hId, info] : database)
		admPacket << hId << info.name << info.isBanned;

	admPacket << uint16_t(clients.size());
	attPacket << uint16_t(clients.size());

	for (const auto& c : clients) {
		std::string ip = c.socket->getRemoteAddress().value().toString() 
			+ ":" + std::to_string(c.socket->getRemotePort());

		admPacket << c.hId << ip << c.id << c.isAttacker << c.isAdmin;
		attPacket << c.hId << ip << c.id << c.isAttacker << c.isAdmin;
	}

	for (const auto& c : clients) {
		if (c.isAdmin)
			auto _ = c.socket->send(admPacket);
		else if (c.isAttacker)
			auto _ = c.socket->send(attPacket);
	}
}
