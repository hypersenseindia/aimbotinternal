#ifndef ADB_UTILS_HPP
#define ADB_UTILS_HPP

#include <Windows.h>
#include <iostream>
#include <Psapi.h>
#include <sstream>
#include <string>

namespace adb {
    std::string GetExecutableDirectory();
    std::string GetExecutableDirectoryPID(DWORD processID);
    bool ChangeDirectory(const std::string& path);
    void TerminateAdbProcesses();
    // Kill BlueStacks/MSI emulator processes and HD-Adb (32-bit) when exiting the panel.
    void KillEmulatorAndAdbOnExit();
    std::string ExtractLibAddress(const std::string& input);
    uintptr_t ConvertToUintPtr(std::string str, int base = 16);
    std::string ExecuteShellCommand(const std::string& firstCommand, const std::string& secondCommand);
    std::string ExecuteShellCommandNoSu(const std::string& command);
    void ExecuteADBCommand(const std::string& command);

    // Multi-port ADB: probe common emulator ports and connect to the first reachable one.
    bool EnsureAdbConnection(const std::string& defaultPort = "5555");
    std::string GetActiveAdbPort();
    std::string GetAdbSerial();
}

#endif