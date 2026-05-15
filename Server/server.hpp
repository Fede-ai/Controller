#include <SFML/Network.hpp>
#include <iostream> 
#include <fstream>
#include <cstdint>
#include <set>
#include <chrono>
#include <sqlite3.h>

struct Client {
	sf::TcpSocket* socket = nullptr;
	bool isAttacker = false, isAdmin = false;
	uint16_t sshId = 0;
	std::string hId = "";
	bool isAwake = true;
};

struct HIdInfo {
	std::string name = "";
	bool isBanned = false;
};

class Server {
public:
	Server();
	int processIncoming();

private:
	inline void printLog(const std::string& s, bool redundant = false) const {
		using namespace std::chrono;
		auto t = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		std::cout << t << " - " << s << "\n";
		
		std::fstream logFile(redundant ? logPathRedundant : logPath, std::ios::app);
		logFile << t << " - " << s << "\n";
		logFile.close();
	}
	
	bool performAwakeCheck();
	void acceptIncoming();
	void handleCommunication(const uint16_t& id, Client& c,
		std::set<std::string>& bannedHIds, std::set<uint16_t>& idsToDisconnect);
	//bool = whether full packet was received, uint16_t = 0 if error, else id assigned
	std::pair<bool, uint16_t> initializeClient(Client& u);

	uint8_t handlePacket(sf::Packet& p, const uint16_t& id, Client& c,
		std::set<std::string>& bannedHIds, std::set<uint16_t>& idsToDisconnect);

	void sendClientList() const;
	void disconnectClient(uint16_t id);
	int initializeDatabase();

	sf::TcpListener listener;
	sf::SocketSelector selector;

	uint16_t nextId = 1;
	sqlite3* db;
	std::map<std::string, HIdInfo> database;
	const std::string adminPassword = "testpass";

	const std::string databasePath = "./database.db";
	const std::string logPath = "./log.log";
	const std::string logPathRedundant = "./redundant.log";

	//in seconds
	size_t lastAwakeCheckTime = 0;
	std::map<uint16_t, Client> clients;
	std::vector<Client> uninitialized;
};