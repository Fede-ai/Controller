#include <Arduino.h>
#include "udpClient.hpp"
#include <vector>

WiFiUDP socket;
unsigned long lastCheck = 0;
std::vector<UdpClient> clients;

void setup() {
  Serial.begin(9600);

  //check for the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Wifi module not reachable");
    while (true) {};
  }
	//check firmware version
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("upgrade the WiFi firmware");
  }

	char ssid[] = "FASTWEB-D4756D", pass[] = "TY62M4NM9M";
  //connect to WiFi network
	int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("connecting to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  
	Serial.print("connected - local ip: ");
	Serial.print(WiFi.localIP());
	Serial.print(", signal strength: ");
	Serial.print(WiFi.RSSI());
	Serial.println(" dBm");

	//bind udp socket to port
  socket.begin(2390);
	lastCheck = millis();
}

void loop() {
  //if there's data available, read a packet
  int packetSize = socket.parsePacket();
  if (packetSize) {		
		char buffer[20];
    int len = socket.read(buffer, 20);
		//r is used to keep the client awake
		//v = new victiv connection
		if (buffer[0] == 'v') {
			UdpClient v;
			v.ip = socket.remoteIP();
			v.port = socket.remotePort()+1;
			std::string time(buffer);
			time.erase(time.begin());
			v.time = stoi(time);
			clients.push_back(v);

			socket.beginPacket(socket.remoteIP(), socket.remotePort());
   	 	socket.write("a");
    	socket.endPacket();

			Serial.print("v: "); 
			Serial.print(v.ip.toString()); 
			Serial.print(":");
			Serial.print(v.port);
			Serial.print(", time: ");
			Serial.println(v.time);
		}

		//flag the client as active
		for (auto& c : clients) {
			if (c.ip == socket.remoteIP() && c.port == socket.remotePort()) {
				c.isActive = true;
				break;
			}
		}
  }

	//check for inactivity
	if (millis() - lastCheck > 10'000) {
		for (int i = 0; i < clients.size(); i++) {
			//disconnect a client if he hasnt been active in 10 secs
			if (!clients[i].isActive) {
				Serial.print(clients[i].ip.toString()); 
				Serial.print(":"); 
				Serial.print(clients[i].port);
				Serial.println(" disconnected due to timeout");
				clients.erase(clients.begin() + i);
				i--;
			}
			else {
				clients[i].isActive = false;
			}
		}
		lastCheck = millis();
	}
}