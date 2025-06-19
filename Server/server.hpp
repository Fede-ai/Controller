#include <SFML/Network.hpp>

struct Client {
	sf::TcpSocket* socket = nullptr;
	bool isAttacker = false, isAdmin = false;
	uint16_t id = 0;
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
	void sendClientList();
	inline static size_t getTime() {
		using namespace std::chrono;
		return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	}

	sf::TcpListener listener;
	sf::SocketSelector selector;

	std::map<std::string, HIdInfo> database;
	const std::string adminPass = "testpass";
	uint16_t nextId = 1;

	std::vector<Client> clients;
	std::vector<Client> uninitialized;
};