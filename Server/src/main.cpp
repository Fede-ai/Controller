#include <Arduino.h>
#include <WiFiS3.h>

int status = WL_IDLE_STATUS;
unsigned int localPort = 2390;
WiFiUDP Udp;

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

	char ssid[] = "marife", pass[] = "cicciociccio";
  //connect to WiFi network
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
  Udp.begin(localPort);
}

void loop() {
  //if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remoteIp = Udp.remoteIP();
    Serial.print(remoteIp);
    Serial.print(", port ");
    Serial.println(Udp.remotePort());

    //read the packet into a buffer
		char buffer[256];
    int len = Udp.read(buffer, 255);
    if (len > 0) {
      buffer[len] = 0;
    }
    Serial.println("Contents:");
    Serial.println(buffer);

    //send a reply, to the IP address and port that sent us the packet we received
		char resp[] = "response";
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(resp);
    Udp.endPacket();
  }
}