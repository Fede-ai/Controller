#pragma once
#include <SFML/Network.hpp>

struct Tunnel
{
public:
	bool isC(sf::TcpSocket s) {
		return (s.getRemoteAddress() == cIp && s.getRemotePort() == cPort);
	}
	bool isV(sf::TcpSocket s) {
		return (s.getRemoteAddress() == vIp && s.getRemotePort() == cPort);
	}
	
	std::string	cIp = "";
	int cPort = 0;
	std::string	vIp = "";
	int vPort = 0;
};