#pragma once
#include <string>

struct Client
{
	std::string ip = "";
	std::string otherIp = "";
	int port = 0;
	int otherPort = 0;
	size_t connectionTime = 0;
};