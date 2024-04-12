#pragma once
#include "client.h"
#include <SFML/Network.hpp>
#include <SFML/Graphics.hpp>
#include "Mlib/System/keyboard.hpp"
#include "Mlib/System/mouse.hpp"
#include "Mlib/System/system.hpp"
#include "Mlib/System/file.hpp"
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
	void startSendingFile();
	sf::Socket::Status sendServer(sf::Packet& p);

	std::vector<Client> controllers, victims, cTemp, vTemp;
	bool isRunning = true, isConnected = false, isPaired = false, isControlling = false;
	bool areSettingsOpen = false, sendKeys = false, sendMouse = false;
	sf::TcpSocket server;
	sf::RenderWindow w;
	const std::string ip = sf::IpAddress::getPublicAddress().toString();
	sf::Uint16 id = 0;
	Mlib::Vec2i lastMousePos = Mlib::Mouse::getPos();
	size_t lastAwakeSignal = Mlib::currentTime().asMil();
	std::string name = "";
	sf::Font font;
	sf::Mutex mutex;
	std::ifstream* file = nullptr;
	int fileState = -1; //-1 = ready, -2 = completed, -3 = failed
	int fileSize = 0;
	std::string ext = "";
};