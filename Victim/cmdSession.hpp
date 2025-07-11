#include <windows.h>
#include <sstream>
#include <exception>
#include <iostream>

class CmdSession {
private:
    HANDLE hStdinWrite;
    HANDLE hStdoutRead;
    PROCESS_INFORMATION pi = {};

public:
    CmdSession() : hStdinWrite(NULL), hStdoutRead(NULL) {
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hStdoutWrite, hStdinRead;

        //create pipes
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) 
            throw std::exception("failed to create out-read pipe");
        if (!SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0)) 
            throw std::exception("failed to set out-read pipe information");

        if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0))
            throw std::exception("failed to create in-read pipe");
        if (!SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0))
            throw std::exception("failed to set in-read pipe information");

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = hStdinRead;
        si.hStdOutput = hStdoutWrite;
        si.hStdError = hStdoutWrite;

        //start cmd.exe
        if (!CreateProcessA(
            NULL,
            (LPSTR)"cmd.exe",
            NULL, NULL, TRUE,
            CREATE_NO_WINDOW,
            NULL, NULL,
            &si, &pi))
            throw std::exception("failed to create cmd.exe process");

        //close unneeded ends
        CloseHandle(hStdinRead);
        CloseHandle(hStdoutWrite);

        std::cout << "cmd session started\n";
    }

    void sendCommand(const std::string& cmd) const {
        DWORD written;
        std::string fullCmd = cmd + "\r\n";

        //write the command to the pipe
        WriteFile(hStdinWrite, fullCmd.c_str(), (DWORD)fullCmd.size(), &written, NULL);
        FlushFileBuffers(hStdinWrite);
    }

    bool readConsoleOutput(std::string& output) const {
        DWORD read;
        char buffer[4096] = {};

        std::stringstream result;
        while (true) {
            BOOL success = ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &read, NULL);
            if (!success || read == 0) break;
            buffer[read] = '\0';
            result << buffer;

            //stop if there's no more data available yet
            if (read < sizeof(buffer) - 1) break;
        }
        output = result.str();
        return output.length() > 0;
    }

    ~CmdSession() {
        if (hStdinWrite) CloseHandle(hStdinWrite);
        if (hStdoutRead) CloseHandle(hStdoutRead);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        std::cout << "cmd session ended\n";
    }
};