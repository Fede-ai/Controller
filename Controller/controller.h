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
	void receiveInfo(); //thread
	void takeCmdInput(); //thread

	void connectServer();
	void updateClientsList(sf::Packet& p);
	static std::string getHardwareId();

	void startSendingFile(); //optional thread

	std::vector<Client> controllers, victims;
	bool isRunning = true, isConnected = false, isPaired = false, isControlling = false;
	bool areSettingsOpen = false, sendKeys = false, sendMouse = false, sharingScreen = false;

	sf::TcpSocket server;
	sf::RenderWindow w;
	const std::string ip = sf::IpAddress::getPublicAddress().toString();
	sf::Uint16 id = 0;
	std::string hardwareId = "";

	Mlib::Vec2i lastMousePos = Mlib::Mouse::getPos();
	size_t lastAwakeSignal = Mlib::currentTime().asMil();
	
	sf::Font font;
	sf::Texture wallpaper;
	sf::Texture screen;

	std::ifstream* file = nullptr;
	int fileState = -1; //-1 = ready, -2 = completed, -3 = failed
	int fileSize = 0;
	std::string ext = "";

	std::pair<sf::Vector2<sf::Uint16>, sf::Vector2<sf::Uint16>> sharingRegion;
};