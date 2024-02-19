#include <WiFiS3.h>

struct UdpClient
{
	IPAddress ip = "";
	IPAddress otherIp = "";
	uint16_t port = 0;
	uint16_t otherPort = 0;
	
	unsigned long time = 0; //time of connection
	bool isActive = true;
	bool victim = true;
};