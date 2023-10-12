#include "server.h"

void Server::initConnection()
{
	int state;
	do {
		state = listener.listen(53000);
		std::cout << "listening status: " << state << "\n";
	} while (state != sf::Socket::Done);

	do {
		state = listener.accept(client);
		std::cout << "acceptance status: " << state << "\n";
	} while (state != sf::Socket::Done);
}

bool Server::control()
{
	while (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad0)) {}
	while (!sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad0)) {}
	while (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad0)) {}

	updateKeys(lastKeys);
	window.create(sf::VideoMode::getDesktopMode(), "Controller", sf::Style::Fullscreen);
	window.setFramerateLimit(40);
	window.clear();
	window.display();
	isMouseEnabled = false, isMouseFixed = false;
	sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
	lastXProp = mousePos.x / window.getSize().x * 10000.f;
	lastYProp = mousePos.y / window.getSize().y * 10000.f;

	while (window.isOpen())
	{
		short state = 0;
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				if (isMouseFixed)
				{
					sf::Packet packet;
					packet << sf::Int16(300);
					state = client.send(packet);
				}
				window.close();
				return 0;
				break;
			case sf::Event::MouseWheelScrolled:
				deltaWheel = event.mouseWheelScroll.delta;
				break;
			}
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad0))
		{
			if (isMouseFixed)
			{
				sf::Packet packet;
				packet << sf::Int16(300);
				state = client.send(packet);
			}
			window.close();
			return 0;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad1))
		{
			if (canEnableMouse)
			{
				isMouseEnabled = !isMouseEnabled;
			}
			canEnableMouse = false;
		}
		else
		{
			canEnableMouse = true;
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad2))
		{
			if (canFixMouse)
			{
				isMouseFixed = !isMouseFixed;

				sf::Packet packet;
				packet << sf::Int16(300);
				state = client.send(packet);
			}
			canFixMouse = false;
		}
		else
		{
			canFixMouse = true;
		}

		if (window.hasFocus())
		{
			sf::Packet packet;
			fillPacket(packet);

			if (packet.getDataSize() > 0)
				state = client.send(packet);

			if (state == sf::Socket::Disconnected)
			{
				window.close();
				return 1;
			}
		}

		window.display();
	}
}

void Server::outputAddresses()
{
	std::string localIp = sf::IpAddress::getLocalAddress().toString();
	std::string publicIp = sf::IpAddress::getPublicAddress().toString();
	std::cout << "local address: " << localIp << ", public address: " << publicIp << "\n";

	while (false)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (localIp != sf::IpAddress::getLocalAddress().toString() || publicIp != sf::IpAddress::getPublicAddress().toString())
		{
			localIp = sf::IpAddress::getLocalAddress().toString();
			publicIp = sf::IpAddress::getPublicAddress().toString();
			std::cout << "local address: " << localIp << ", public address: " << publicIp << "\n";
		}
	}
}

void Server::fillPacket(sf::Packet& packet)
{
	short keys[256];
	updateKeys(keys);

	/*
	if ((keys[0x41] & 0x01) && lastKeys[0x41] == 0)
		std::cout << "pressed\n";
	if (keys[0x41] == 0 && lastKeys[0x41] != 0)
		std::cout << "released\n";
	*/

	for (int i = 0; i < 256; i++)
	{
		if (!isMouseEnabled && (i == 1 || i == 2 || i == 4 || i == 5 || i == 6))
			continue;

		if (i == 0 || i == 7 || (i >= 96 && i <= 105)) {}
		//key-stroke registered
		else if (keys[i] & 0x01)
		{
			packet << (sf::Int16)i;
			packet << true;
		}
		//key release registered
		else if (keys[i] == 0 && lastKeys[i] != 0)
		{
			packet << (sf::Int16)i;
			packet << false;
		}
		lastKeys[i] = keys[i];
	}

	sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
	sf::Int16 xProp = mousePos.x / window.getSize().x * 10000.f;
	sf::Int16 yProp = mousePos.y / window.getSize().y * 10000.f;
	if (isMouseEnabled)
	{
		if (xProp != lastXProp || yProp != lastYProp)
		{
			packet << (sf::Int16)0;
			packet << xProp;
			packet << yProp;

		}

		if (deltaWheel != 0)
		{
			packet << (sf::Int16)7;
			packet << deltaWheel;
		}
	}			
	lastXProp = xProp;
	lastYProp = yProp;
	deltaWheel = 0;
}

void Server::updateKeys(short* keys)
{
	for (int i = 0; i < 256; i++)
	{
		keys[i] = GetAsyncKeyState(i);
	}
}