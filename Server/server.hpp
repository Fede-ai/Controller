#include <SFML/Network.hpp>
#include <iostream> 
#include <fstream>

struct Client {
	sf::TcpSocket* socket = nullptr;
	bool isAttacker = false, isAdmin = false;
	uint16_t sshId = 0;
	std::string hId = "";
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
	inline void outputLog(std::string s) const {		
		using namespace std::chrono;
		auto t = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		std::cout << t << " - " << s << "\n";

		std::fstream logFile(logPath, std::ios::app);
		logFile << t << " - " << s << "\n";
		logFile.close();
	}

	void sendClientList() const;
	void disconnectClient(uint16_t id);
	int loadDatabase();
	void saveDatabase() const;

	sf::TcpListener listener;
	sf::SocketSelector selector;

	std::map<std::string, HIdInfo> database;
	const std::string adminPass = "testpass";
	const std::string databasePath = "./database.txt";
	const std::string logPath = "./log.log";
	uint16_t nextId = 1;

	std::map<uint16_t, Client> clients;
	std::map<uint16_t, Client> uninitialized;
};