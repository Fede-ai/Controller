/*void Server::run()
{
	outputAddresses();
	outputInstructions();

	while (true)
	{
		bool canControl = false;
		initConnection();
		isConnected = true;

		while (isConnected)
		{
			//check if still connected once per second
			if (getTime() != lastCheck)
			{
				sf::Packet packet;
				packet << (sf::Int16)299;
				lastCheck = getTime();
				isConnected = (client.send(packet) == sf::Socket::Done);
			}
			//open control window
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad0))
			{
				if (canControl)
					remoteControl();
				canControl = false;
			}
			else
			{
				canControl = true;
			}
		}
		std::cout << "client disconnected\n\n";
	}
}
void Server::initConnection()
{
	int state;
	do {
		state = listener.listen(53000);
		std::cout << "listening status: " << state << ", port n. 53000" << "\n";
	} while (state != sf::Socket::Done);

	bool clientAccepted = false;
	while (!clientAccepted)
	{
		do {
			state = listener.accept(client);		
			std::cout << "acceptance status: " << state << "\n";
		} while (state != sf::Socket::Done);
		
		std::cout << "incoming connection from: " << client.getRemoteAddress() << "\n";
		std::cout << "do you want to proceed with this client [Y/N]? ";

		std::string cmd = "";
		do {
			std::cin >> cmd;
		} while (cmd != "Y" && cmd != "y" && cmd != "N" && cmd != "n");
		if (cmd == "Y" || cmd == "y")
		{
			std::cout << "client accepted, waiting for commands\n";
			clientAccepted = true;
		}
		else
		{
			std::cout << "client disconnected\n\n";
			client.disconnect();
		}
	}
}
void Server::remoteControl()
{
	updateKeys(lastKeys);
	window.create(sf::VideoMode::getDesktopMode(), "Controller", sf::Style::Fullscreen);
	window.setFramerateLimit(40);

	isMouseEnabled = false, canEnableMouse = false;
	bool isMouseFixed = false, canFixMouse = false;
	bool canClose = false;

	sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
	lastXProp = mousePos.x / window.getSize().x * 10000.f;
	lastYProp = mousePos.y / window.getSize().y * 10000.f;

	while (window.isOpen())
	{
		window.clear(sf::Color(50, 50, 50));
		sf::Packet packet;
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type)
			{
			case sf::Event::Closed:
				window.close();
				break;
			case sf::Event::MouseWheelScrolled:
				if (isMouseEnabled)
				{
					packet << (sf::Int16)7;
					packet << (sf::Int8)event.mouseWheelScroll.delta;
				}
				break;
			}
		}

		//close window
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad0))
		{
			if (canClose && window.hasFocus())
			{
				window.close();
			}
			canClose = false;
		}
		else
		{
			canClose = true;
		}

		//block mouse inputs
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad1))
		{
			if (canEnableMouse && window.hasFocus())
			{
				isMouseEnabled = !isMouseEnabled;
			}
			canEnableMouse = false;
		}
		else
		{
			canEnableMouse = true;
		}

		//fix mouse position
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad2))
		{
			if (canFixMouse && window.hasFocus())
			{
				isMouseFixed = !isMouseFixed;
				packet << sf::Int16(300);
			}
			canFixMouse = false;
		}
		else
		{
			canFixMouse = true;
		}

		//select and send a file
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad3))
		{
			if (canLoadFile && window.hasFocus())
			{
				//the info would be sent after the file, just delete it
				packet.clear();

				window.close();

				selectAndSendFile();

				window.create(sf::VideoMode::getDesktopMode(), "Controller", sf::Style::Fullscreen);
				window.setFramerateLimit(40);
			}
			canLoadFile = false;
		}
		else
		{
			canLoadFile = true;
		}

		//disconnect client
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad4))
		{
			if (canDisconnect && window.hasFocus())
			{
				client.disconnect();
				isConnected = false;
			}
			canDisconnect = false;
		}
		else
		{
			canDisconnect = true;
		}

		//close client program
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad7) &&
			sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad8) &&
			sf::Keyboard::isKeyPressed(sf::Keyboard::Numpad9))
		{
			packet << (sf::Int16)302;	
			client.send(packet);

			client.disconnect();
			isConnected = false;
		}

		if (window.hasFocus())
			fillPacket(packet);

		//control if still connected at least once per second
		if (getTime() != lastCheck)
		{
			packet << (sf::Int16)299;
			lastCheck = getTime();
		}		
		
		if (packet.getDataSize() > 0)
			isConnected = (client.send(packet) == sf::Socket::Done);

		if (!isConnected)
			window.close();

		window.display();
	}

	//set free the mouse if you stop controlling
	if (isMouseFixed)
	{
		sf::Packet packet;
		packet << (sf::Int16)300;
		client.send(packet);
	}
}

void Server::outputAddresses()
{
	std::string localIp = sf::IpAddress::getLocalAddress().toString();
	std::string publicIp = sf::IpAddress::getPublicAddress().toString();
	std::cout << "local address: " << localIp << ", public address: " << publicIp << "\n\n";

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
void Server::outputInstructions()
{
	std::cout << "here are the commands with the associated numpad keys\n";
	std::cout << "(remember to activate the bloc num): \n";
	std::cout << " - numpad 0 => open control window\n";
	std::cout << " - numpad 1 => enable mouse control\n";
	std::cout << " - numpad 2 => fix mouse's position\n";
	std::cout << " - numpad 3 => select and send file\n";
	std::cout << " - numpad 4 => disconnect from current client\n";
	std::cout << " - numpad 7+8+9 => shut down current client\n\n";
}

void Server::fillPacket(sf::Packet& packet)
{
	short keys[256];
	updateKeys(keys);

	
	//if ((keys[0x41] & 0x01) && lastKeys[0x41] == 0)
	//	std::cout << "pressed\n";
	//if (keys[0x41] == 0 && lastKeys[0x41] != 0)
	//	std::cout << "released\n";
	

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
	if (isMouseEnabled && (xProp != lastXProp || yProp != lastYProp))
	{
		packet << (sf::Int16)0;
		packet << xProp;
		packet << yProp;
	}			
	lastXProp = xProp;
	lastYProp = yProp;
}
void Server::selectAndSendFile()
{
	sf::Packet packet;
	OPENFILENAMEA file;
	char path[100];

	ZeroMemory(&file, sizeof(file));
	file.lStructSize = sizeof(file);
	file.hwndOwner = NULL;
	file.lpstrFile = path;
	file.lpstrFile[0] = '\0';
	file.nMaxFile = sizeof(path);
	file.lpstrFilter = (PSTR)"All Files (*.*)\0*.*\0";
	file.nFilterIndex = 1;
	file.lpstrFileTitle = NULL;
	file.nMaxFileTitle = 0;
	file.lpstrInitialDir = NULL;
	file.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameA(&file))
	{
		std::string extention, pathString = path;
		size_t lastDotPos = pathString.find_last_of('.');
		if (lastDotPos != std::string::npos)
			extention = pathString.substr(lastDotPos + 1);

		packet << (sf::Int16)301;
		packet << extention;
		client.send(packet);

		std::ifstream file(pathString, std::ios::binary);
		if (file.is_open()) 
		{
			packet.clear();
			std::vector<char> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			packet.append(&fileData[0], fileData.size());

			client.send(packet);
			file.close();
		}
	}
}

void Server::updateKeys(short* keys)
{
	for (int i = 0; i < 256; i++)
	{
		keys[i] = GetAsyncKeyState(i);
	}
}
size_t Server::getTime()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}*/
