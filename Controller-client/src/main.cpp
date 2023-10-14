#include <SFML/Network.hpp>
#include <fstream>
#include <iostream>
#include <windows.h>
#include <thread>
#include <chrono>

std::string takeInfo(bool& erase);
void simulate(sf::Int16 cmd, bool info);
void fixMouse(sf::Vector2f& pos, bool& isFixed);
void saveFile(sf::Packet packet, std::string extention);

int main()
{
    sf::Vector2f screenDim;
    sf::Vector2f mousePos = sf::Vector2f(0, 0);
    bool isMouseFixed = false;
    RECT desktop;
    const HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    screenDim.x = desktop.right;
    screenDim.y = desktop.bottom;

    std::thread fixMouseThread(fixMouse, std::ref(mousePos), std::ref(isMouseFixed));
    fixMouseThread.detach();

    bool erase = false;
    bool end = false;
    sf::IpAddress address = takeInfo(erase);

    while (!end)
    {
        sf::TcpSocket server;

        while(server.connect(address, 53000) != sf::Socket::Done) {}
    
        sf::Packet packet;
        while(server.receive(packet) == sf::Socket::Done)
        {
            sf::Int16 cmd;
            while(!packet.endOfPacket())
            {
                packet >> cmd;
                if (cmd == 299) {}
                //fix or free mouse
                else if (cmd == 300)
                {
                    isMouseFixed = !isMouseFixed;
                }
                //download and save file
                else if (cmd == 301)
                {
                    std::string extention;
                    packet >> extention;
                    server.receive(packet);
                    std::thread saveFileThread(saveFile, packet, extention);
                    saveFileThread.detach();
                    break;
                }
                //terminate program
                else if (cmd == 302)
                {
                    server.disconnect();
                    end = true;
                    break;
                }
                //mouse movement (info = Int8)
                else if (cmd == 0)
                {   
                    sf::Int16 xProp;
                    packet >> xProp;
                    sf::Int16 yProp;
                    packet >> yProp;
                    float xPos = xProp / 10000.f * screenDim.x;
                    float yPos = yProp / 10000.f * screenDim.y;
                    mousePos = sf::Vector2f(xPos, yPos);

                    SetCursorPos(xPos, yPos);
                }
                //wheel scroll input (info = Int8)
                else if (cmd == 7)
                {
                    sf::Int8 info;
                    packet >> info;
                    INPUT in;
                    in.type = INPUT_MOUSE;
                    in.mi.time = 0;
                    in.mi.mouseData = info * 120;
                    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
                    SendInput(1, &in, sizeof(INPUT));
                }
                //general input (info = bool)
                else
                {
                    bool info;
                    packet >> info;
                    simulate(cmd, info);
                }
            }    
        }
        isMouseFixed = false;
    }

    return 0;
}

std::string takeInfo(bool& erase)
{
    std::fstream file;    
    std::string address;
    file.open("info.txt", std::ios::in | std::ios::out);
    getline(file, address); 

    while (address.at(address.length()-1) == ' ')
    {
        address.erase(address.length()-1);
    }
    erase = address.at(address.length()-1) == 'e';
    address.erase(address.length()-1);

    file.close();
    return address;
}
void simulate(sf::Int16 cmd, bool info)
{  
    //left button event
    if (cmd == 1)
    {
        INPUT in;
        in.type = INPUT_MOUSE;
        in.mi.time = 0;
        in.mi.mouseData = 0;

        if (info)
        {
            in.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &in, sizeof(INPUT));
        }
        else
        {
            in.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &in, sizeof(INPUT));        
        }
    }
    //right button event
    else if (cmd == 2)
    {
        INPUT in;
        in.type = INPUT_MOUSE;
        in.mi.time = 0;
        in.mi.mouseData = 0;

        if (info)
        {
            in.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
            SendInput(1, &in, sizeof(INPUT));
        }
        else
        {
            in.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
            SendInput(1, &in, sizeof(INPUT));        
        }
    }
    //middle button event
    else if (cmd == 4)
    {
        INPUT in;
        in.type = INPUT_MOUSE;
        in.mi.time = 0;
        in.mi.mouseData = 0;

        if (info)
        {
            in.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
            SendInput(1, &in, sizeof(INPUT));
        }
        else
        {
            in.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
            SendInput(1, &in, sizeof(INPUT));        
        }
    }
    //side button 1 event
    else if (cmd == 5)
    {
        INPUT in;
        in.type = INPUT_MOUSE;
        in.mi.time = 0;
        in.mi.mouseData = XBUTTON1;

        if (info)
        {
            in.mi.dwFlags = MOUSEEVENTF_XDOWN;
            SendInput(1, &in, sizeof(INPUT));
        }
        else
        {
            in.mi.dwFlags = MOUSEEVENTF_XUP;
            SendInput(1, &in, sizeof(INPUT));        
        }
    }
    //side button 2 event
    else if (cmd == 6)
    {
        INPUT in;
        in.type = INPUT_MOUSE;
        in.mi.time = 0;
        in.mi.mouseData = XBUTTON2;

        if (info)
        {
            in.mi.dwFlags = MOUSEEVENTF_XDOWN;
            SendInput(1, &in, sizeof(INPUT));
        } 
        else
        {
            in.mi.dwFlags = MOUSEEVENTF_XUP;
            SendInput(1, &in, sizeof(INPUT));        
        }   
    }
    //keyboard event
    else if (cmd == 3 || cmd >= 8)
    {
        INPUT in;
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = 0; // hardware scan code for key
        in.ki.time = 0;
        in.ki.dwExtraInfo = 0;
    
        if (info)
        {
            in.ki.wVk = cmd;
            in.ki.dwFlags = 0;
            SendInput(1, &in, sizeof(INPUT));        
        }
        else
        {
            in.ki.wVk = cmd;
            in.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &in, sizeof(INPUT));        
        }
    }
}
void fixMouse(sf::Vector2f& pos, bool& isFixed)
{
    while (true)
    {
        if (isFixed)
        {
            SetCursorPos(pos.x, pos.y);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void saveFile(sf::Packet packet, std::string extention)
{
    std::vector<char> fileData;
    const void* data = packet.getData();
    std::size_t dataSize = packet.getDataSize();
    fileData.insert(fileData.end(), static_cast<const char*>(data), static_cast<const char*>(data) + dataSize);
    
    OPENFILENAMEA file;
    char path[100];

    ZeroMemory(&file, sizeof(file));
    file.lStructSize = sizeof(file);
    file.hwndOwner = NULL;
    file.lpstrFilter = std::string("Sent Filetype (*." + extention + ")").c_str();
    file.lpstrFile = path;
    file.lpstrFile[0] = '\0';
    file.nMaxFile = sizeof(path);
    file.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileNameA(&file))
    {
        std::ofstream receivedFile(std::string(path) + "." + extention, std::ios::binary);
        receivedFile.write(&fileData[0], fileData.size());
        receivedFile.close();        
    }
}