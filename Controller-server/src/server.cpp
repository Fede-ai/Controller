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

short Server::control()
{
	updateKeys(lastKeys);
	short state = 0;

	window.create(sf::VideoMode(), "Controller", sf::Style::Fullscreen);
	window.setFramerateLimit(60);
	window.clear();
	window.display();
	
	sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
	lastXProp = mousePos.x / window.getSize().x * 10000.f;
	lastYProp = mousePos.y / window.getSize().y * 10000.f;
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::MouseWheelScrolled:
				deltaWheel = event.mouseWheelScroll.delta;
				break;
			}
		}

		sf::Packet packet;
		fillPacket(packet);

		if (packet.getDataSize() > 0)
			state = client.send(packet);

		if (state == sf::Socket::Disconnected)
			window.close();

		window.display();
	}

	return state;
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

	for (int i = 0; i < 256; i++)
	{
		if (i == 0 || i == 7) {}
		//key-stroke registered
		else if (keys[i] & 0x01)
		{
			packet << (sf::Int16)i;
			packet << true;
		}
		//key release registered
		else if (keys[i] == 0 && (lastKeys[i] & 32768))
		{
			packet << (sf::Int16)i;
			packet << false;
		}
		lastKeys[i] = keys[i];
	}

	sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
	sf::Int16 xProp = mousePos.x / window.getSize().x * 10000.f;
	sf::Int16 yProp = mousePos.y / window.getSize().y * 10000.f;
	if (xProp != lastXProp || yProp != lastYProp)
	{
		packet << (sf::Int16)0;
		packet << xProp;
		packet << yProp;
		lastXProp = xProp;
		lastYProp = yProp;
	}

	if (deltaWheel != 0)
	{
		packet << (sf::Int16)7;
		packet << deltaWheel;
		deltaWheel = 0;
	}
}

void Server::updateKeys(short* keys)
{
	for (int i = 0; i < 256; i++)
	{
		keys[i] = GetAsyncKeyState(i);
	}
}