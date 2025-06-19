#include "attacker.hpp"
#include <sstream>
#include <conio.h>

static constexpr uint32_t hash(const std::string s) noexcept {
	uint32_t hash = 5381;
	for (const char* c = s.c_str(); c < s.c_str() + s.size(); ++c)
		hash = ((hash << 5) + hash) + (unsigned char)*c;
	return hash;
}

Attacker::Attacker(std::string inHId)
	:
	myHId(inHId)
{
	receiveThread = new std::thread(&Attacker::receiveTcp, this);
}

int Attacker::runConsoleShell()
{
	bool exit = false;
	while (true) {
		printMsg("\033[32m" + myHId + "> \033[0m");
		std::string line, cmd;
		getline(std::cin, line);

		if (exit || std::cin.eof())
			break;

		consoleMemory << line << "\n";
		std::stringstream ss(line);
		ss >> cmd;

		switch (hash(cmd)) {
		case hash("exit"): {
			exit = true;
			break;
		}		
		case hash("clear"): {
			consoleMemory = std::stringstream();
			needRefreshConsole = true;
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
				printErr("client is not initialized\n");
				break;
			}
			else if (!isAdmin) {
				printErr("admin privileges are needed\n");
				break;
			}

			std::string oHId, name;
			ss >> oHId >> name;			
			if (oHId.empty() || name.empty()) {
				printErr("invalid arguments\n");
				break;
			}

			sf::Packet req; 
			req << uint16_t(0) << uint8_t(Cmd::CHANGE_NAME) << oHId << name;
			if (server.send(req) != sf::Socket::Status::Done)
				printErr("failed to send request to server\n");
			break;
		}
		case hash("ban"): {
			if (!isInitialized) {
				printErr("client is not initialized\n");
				break;
			}
			else if (!isAdmin) {
				printErr("admin privileges are needed\n");
				break;
			}

			std::string oHId;
			ss >> oHId;
			if (oHId.empty()) {
				printErr("invalid arguments\n");
				break;
			}

			sf::Packet req;
			req << uint16_t(0) << uint8_t(Cmd::BAN_HID) << oHId;
			if (server.send(req) != sf::Socket::Status::Done)
				printErr("failed to send request to server\n");
			break;
		}
		case hash("unban"): {
			if (!isInitialized) {
				printErr("client is not initialized\n");
				break;
			}
			else if (!isAdmin) {
				printErr("admin privileges are needed\n");
				break;
			}

			std::string oHId;
			ss >> oHId;
			if (oHId.empty()) {
				printErr("invalid arguments\n");
				break;
			}
			
			sf::Packet req;
			req << uint16_t(0) << uint8_t(Cmd::UNBAN_HID) << oHId;
			if (server.send(req) != sf::Socket::Status::Done)
				printErr("failed to send request to server\n");
			break;
		}
		case hash("kill"): {
			if (!isInitialized) {
				printErr("client is not initialized\n");
				break;
			}
			else if (!isAdmin) {
				printErr("admin privileges are needed\n");
				break;
			}

			uint16_t oId = uint16_t(0);
			ss >> oId;
			if (oId == uint16_t(0)) {
				printErr("invalid arguments\n");
				break;
			}

			sf::Packet req;
			req << uint16_t(0) << uint8_t(Cmd::KILL) << oId;
			if (server.send(req) != sf::Socket::Status::Done)
				printErr("failed to send request to server\n");
			break;
		}
		case hash(""): break;
		default: {
			printErr("enter a valid command\n");
			break;
		}
		}

		if (needRefreshConsole) {
			system("cls");
			std::cout << listMemory.str() << consoleMemory.str();
		}
	}

	server.disconnect();
	receiveThread->detach();

	return 0;
}

void Attacker::receiveTcp()
{
	while (true) {
		sf::Packet p;
		auto status = server.receive(p);
		//wait before retrying
		if (status == sf::Socket::Status::Disconnected) {
			if (isInitialized) {
				printErr("[disconnected from server]");
				listMemory = std::stringstream();
				isInitialized = false;
				isAdmin = false;
			}
			sf::sleep(sf::milliseconds(100)); 
		}
		if (status != sf::Socket::Status::Done)
			continue;

		uint16_t reqId;
		p >> reqId;

		//another thread is going to handle the response
		if (reqId != uint16_t(0)) {
			pendingResponses[reqId] = p;
			continue;
		}

		uint8_t cmd;
		p >> cmd;

		if (cmd == uint8_t(Cmd::LIST_UPDATE))
			updateList(p);
	}
}

void Attacker::connectServer(std::stringstream& ss, bool pw)
{
	isInitialized = false;
	isAdmin = false;
	server.disconnect();

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
			printErr("port provided is not valid\n");
			return;
		}
		ipStr = ipStr.substr(0, pos);
	}

	//parse the ip address
	auto ip = sf::IpAddress::resolve(ipStr);
	if (!ip.has_value()) {
		printErr("ip address provided is not valid\n");
		return;
	}
	serverIp = ip.value();
	isInitialized = false;

	//connect to the server
	if (server.connect(serverIp, port) != sf::Socket::Status::Done) {
		printErr("failed to connect to server\n");
		return;
	}

	//take password input from the user (if admin)
	std::string password = "";
	if (pw) {
		printMsg("enter the password: ");
		while (true) {
			int lastChar = getch();
			if (lastChar == 13) {
				printMsg("\n");
				break;
			}
			if (lastChar == 8 && !password.empty()) {
				password = password.substr(0, password.length() - 1);
				printMsg("\b \b");
			}
			else if (lastChar >= 33 && lastChar <= 125) {
				password += char(lastChar);
				printMsg("*");
			}
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
		printErr("failed to send registration request\n");
		server.disconnect();
		return;
	}

	//wait for initialization response and output progress
	printMsg("waiting for initialization: [");
	for (int i = 0; i < 10; i++) {
		if (pendingResponses.find(reqId) == pendingResponses.end()) {
			printMsg("*");
			sf::sleep(sf::milliseconds(500));
		}
		else
			printMsg(".");
	}
	printMsg("] - ");

	//initialization wasn't successful
	if (pendingResponses.find(reqId) == pendingResponses.end()) {
		printErr("timed out\n");
		server.disconnect();
		return;
	}
	
	sf::Packet res = pendingResponses[reqId];
	pendingResponses.erase(reqId);

	bool success = false;
	res >> success;
	if (!success) {
		if (pw)
			printErr("failed admin login\n");
		else
			printErr("you are banned :(\n");
		server.disconnect();
		return;
	}

	res >> myId;
	isInitialized = true;
	isAdmin = pw;

	if (pw)
		printMsg("admin access granted (" + std::to_string(myId) + ")\n");
	else
		printMsg("initialized (" + std::to_string(myId) + ")\n");
}

void Attacker::updateList(sf::Packet& p)
{
	listMemory = std::stringstream();
	listMemory << "\033[36m";
	uint16_t dbSize, clientsSize;

	p >> dbSize;
	//database is only sent to admins
	if (dbSize > 0) {
		listMemory << "DATABASE:\n";
		for (int i = 0; i < dbSize; i++) {
			std::string oHId = "", name = "";
			bool ban = false;
			p >> oHId >> name >> ban;
			listMemory << " - " << oHId << " (" << name << "): ";
			if (ban)
				listMemory << "banned\n";
			else
				listMemory << "not banned\n";
		}
		listMemory << "\n";
	}

	p >> clientsSize;
	listMemory << "CLIENTS:\n";
	for (int i = 0; i < clientsSize; i++) {
		std::string oHId = "", ip = "";
		uint16_t id = 0;
		bool att = false, adm = false;
		p >> oHId >> ip >> id >> att >> adm;

		listMemory << " - " << oHId << " (" << id << "): ";
		if (adm)
			listMemory << "admin - " << ip;
		else if (att)
			listMemory << "attacker - " << ip;
		else
			listMemory << "victim - " << ip;

		listMemory << "\n";
	}
	listMemory << "\033[0m\n";

	needRefreshConsole = true;
	std::cout << "\033[31m@\033[0m";
}
