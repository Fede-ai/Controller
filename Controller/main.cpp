#include <thread>
#include "controller.h"

/* possible messages form server:
c: controller accepted
n: controller info
l: victim info
e: exit
r: erase clients list
d: display clients list
p: paired with 
*/

int main()
{
    sf::TcpSocket ser;
    sf::Socket::Status status;
    do {
        status = ser.connect(SERVER_IP, SERVER_PORT);
    } while (status != sf::Socket::Done);
    std::cout << "connected with server\n";
    
    char code = '-';
    do {
        std::string str = "c";
        sf::Packet p;
        p << str;
        if (ser.send(p) != sf::Socket::Done)
            continue;

        p.clear();
        if (ser.receive(p) != sf::Socket::Done)
            continue;

        if ((p >> str) && str.size() == 1)
            code = str.at(0);
    } while (code != 'c');
    std::cout << "connection with server approved\n";

    return 0;

    //Controller controller(&ard);
    //std::thread receive(&Controller::receiveInfo, &controller);
    //receive.detach();
    //std::thread input(&Controller::takeCmdInput, &controller);
    //input.detach();
    //
    //int code = controller.controlWindow();
	//return code;
}	