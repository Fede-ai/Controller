#pragma once
#include "client.h"
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include "Mlib/Util/util.hpp"
#include "Mlib/Io/keyboard.hpp"
#include "Mlib/Io/mouse.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include "../secret.h"

class Controller
{
public:
	Controller();
	void controlWindow();

private:
	void receiveInfo();
	void takeCmdInput();
	void connectServer();
	void displayList();

	std::vector<Client> controllers, victims;
	bool isRunning = true, isConnected = false, isPaired = false, isControlling = false;
	const Mlib::Vec2i screenSize = Mlib::displaySize();
	sf::TcpSocket server;
	sf::RenderWindow w;
	const std::string ip = sf::IpAddress::getPublicAddress().toString();
	sf::Uint16 id = 0;
	Mlib::Vec2i lastMousePos = Mlib::Mouse::getPos();
	size_t lastAwakeSignal = Mlib::getTime();
	std::string name = "";
};

