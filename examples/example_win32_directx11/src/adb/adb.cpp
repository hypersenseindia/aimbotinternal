#include "adb.hpp"
#include "ProcessUtils.hpp"
#include <TlHelp32.h>
#include <vector>
#include <algorithm>

using namespace ProcessUtils;

namespace {

    std::string g_activeAdbPort;
    std::string g_adbSerial;

    static const char* kCommonAdbPorts[] = {
        "5555", "5554", "5556", "5557", "5558",
        "5562", "5565", "5572", "5575", "5582", "5585",
        "5592", "5595", "6767", "6969"
    };

    int RunHdAdbRaw(const std::string& adbArgs, std::string* output = nullptr) {
        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hStdOutRead = nullptr;
        HANDLE hStdOutWrite = nullptr;
        if (output) {
            if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0))
                return -1;
            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
            si.hStdOutput = hStdOutWrite;
            si.hStdError = hStdOutWrite;
            si.dwFlags |= STARTF_USESTDHANDLES;
        }

        std::string cmdLine = ".\\HD-Adb " + adbArgs;
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        if (!CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, output ? TRUE : FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            if (hStdOutWrite) CloseHandle(hStdOutWrite);
            if (hStdOutRead) CloseHandle(hStdOutRead);
            return -1;
        }

        if (hStdOutWrite)
            CloseHandle(hStdOutWrite);

        if (output && hStdOutRead) {
            char buffer[512];
            DWORD readBytes = 0;
            while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &readBytes, nullptr) && readBytes > 0) {
                buffer[readBytes] = '\0';
                *output += buffer;
            }
            CloseHandle(hStdOutRead);
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return static_cast<int>(exitCode);
    }

    std::string BuildHdAdbCommand(const std::string& adbTail) {
        if (g_adbSerial.empty())
            return std::string(".\\HD-Adb ") + adbTail;
        return std::string(".\\HD-Adb -s ") + g_adbSerial + " " + adbTail;
    }

    bool AdbConnectOutputOk(const std::string& output) {
        return output.find("connected") != std::string::npos ||
            output.find("already connected") != std::string::npos ||
            output.find("Connected to") != std::string::npos;
    }

    bool TryAdbConnect(const std::string& port) {
        const std::string target = "127.0.0.1:" + port;
        std::string output;
        RunHdAdbRaw("connect " + target, &output);

        if (AdbConnectOutputOk(output)) {
            g_activeAdbPort = port;
            g_adbSerial = target;
            return true;
        }
        return false;
    }

    bool FindAndConnectAdbPort(const std::string& defaultPort) {
        std::vector<std::string> ports;
        ports.push_back(defaultPort.empty() ? "5555" : defaultPort);

        for (const char* port : kCommonAdbPorts) {
            if (std::find(ports.begin(), ports.end(), port) == ports.end())
                ports.push_back(port);
        }

        for (size_t i = 0; i < ports.size(); ++i) {
            if (TryAdbConnect(ports[i])) {
                std::cerr << "[ADB] Connected on port " << ports[i] << std::endl;
                return true;
            }

            if (i + 1 < ports.size()) {
                std::cerr << "[ADB] Port " << ports[i]
                    << " failed, trying port " << ports[i + 1] << "..." << std::endl;
            }
        }

        std::cerr << "[ADB] Unreachable - no emulator port responded." << std::endl;
        return false;
    }

    void TerminateProcessesByName(const wchar_t* exeName) {
        if (!exeName || !*exeName)
            return;

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, exeName) == 0) {
                    HANDLE hProc =
                        OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        TerminateProcess(hProc, 0);
                        CloseHandle(hProc);
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    void HiddenTaskKill(const char* imageName) {
        if (!imageName || !*imageName)
            return;

        STARTUPINFOA si = {};
        PROCESS_INFORMATION pi = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        std::string cmd = std::string("taskkill /IM ") + imageName + " /F /T";
        std::vector<char> cmdLine(cmd.begin(), cmd.end());
        cmdLine.push_back('\0');

        if (CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

} // namespace

namespace adb {

    std::string GetExecutableDirectory() {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);

        std::string fullPath(path);
        size_t lastSlashIndex = fullPath.find_last_of("\\/");
        if (lastSlashIndex != std::string::npos) {
            return fullPath.substr(0, lastSlashIndex);
        }
        return "";
    }

    std::string GetExecutableDirectoryPID(DWORD processID) {
        char path[MAX_PATH];
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

        if (hProcess) {
            if (GetModuleFileNameExA(hProcess, NULL, path, MAX_PATH)) {
                std::string fullPath(path);
                size_t lastSlashIndex = fullPath.find_last_of("\\/");
                CloseHandle(hProcess);
                if (lastSlashIndex != std::string::npos) {
                    return fullPath.substr(0, lastSlashIndex);
                }
            }
            CloseHandle(hProcess);
        }

        return "";
    }

    bool ChangeDirectory(const std::string& path) {
        return SetCurrentDirectoryA(path.c_str());
    }

    void TerminateAdbProcesses() {
        DWORD ProcIdAdb = GetProc(L"adb.exe");
        DWORD ProcIdAdb2 = 0;
        KillProc(ProcIdAdb);
        if (IsProcRun(L"adb.exe", ProcIdAdb2)) {
            ForgeKillProc(ProcIdAdb2);
        }

        DWORD ProcIdAdbHD = GetProc(L"HD-Adb.exe");
        DWORD ProcIdAdbHD2 = 0;
        KillProc(ProcIdAdbHD);
        if (IsProcRun(L"HD-Adb.exe", ProcIdAdbHD2)) {
            ForgeKillProc(ProcIdAdbHD2);
        }
    }

    void KillEmulatorAndAdbOnExit() {
        static const wchar_t* kEmulatorProcesses[] = {
            L"HD-Player.exe",
            L"HD-Adb.exe",
            L"HD-ADB.exe",
            L"adb.exe",
            L"BlueStacks.exe",
            L"Bluestacks.exe",
            L"BstkSVC.exe",
            L"BlueStacks_nxt.exe",
            L"MSIAppPlayer.exe",
            L"HD-Frontend.exe",
            L"HD-MultiInstanceManager.exe",
            L"HD-Agent.exe",
            L"HD-LogCollector.exe",
        };

        for (const wchar_t* exe : kEmulatorProcesses)
            TerminateProcessesByName(exe);

        TerminateAdbProcesses();

        HiddenTaskKill("HD-Adb.exe");
        HiddenTaskKill("HD-ADB.exe");
        HiddenTaskKill("adb.exe");
        HiddenTaskKill("HD-Player.exe");
        HiddenTaskKill("BlueStacks.exe");
        HiddenTaskKill("Bluestacks.exe");
        HiddenTaskKill("BstkSVC.exe");
        HiddenTaskKill("BlueStacks_nxt.exe");
        HiddenTaskKill("MSIAppPlayer.exe");
    }

    std::string ExtractLibAddress(const std::string& input) {
        std::size_t pos = input.find('-');
        if (pos != std::string::npos && pos >= 8) {
            return input.substr(pos - 8, 8);
        }
        return "";
    }

    uintptr_t ConvertToUintPtr(std::string str, int base) {
        if (sizeof(uintptr_t) == sizeof(unsigned long)) {
            return strtoul(str.c_str(), nullptr, base);
        }
        return strtoull(str.c_str(), nullptr, base);
    }

    std::string ExecuteShellCommand(const std::string& firstCommand, const std::string& secondCommand) {
        HANDLE hStdOutRead, hStdOutWrite;
        HANDLE hStdInRead, hStdInWrite;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
            std::cerr << "CreatePipe failed: " << GetLastError() << std::endl;
            return "";
        }

        if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "SetHandleInformation failed: " << GetLastError() << std::endl;
            return "";
        }

        if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
            std::cerr << "CreatePipe failed: " << GetLastError() << std::endl;
            return "";
        }

        if (!SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "SetHandleInformation failed: " << GetLastError() << std::endl;
            return "";
        }

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdError = hStdOutWrite;
        si.hStdOutput = hStdOutWrite;
        si.hStdInput = hStdInRead;
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        const std::string adbShellCmd = BuildHdAdbCommand("shell");
        std::vector<char> adbShellBuf(adbShellCmd.begin(), adbShellCmd.end());
        adbShellBuf.push_back('\0');

        if (!CreateProcessA(NULL, adbShellBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
            return "";
        }

        CloseHandle(hStdOutWrite);
        CloseHandle(hStdInRead);

        DWORD written;
        std::string commands = firstCommand + "\n" + secondCommand + "\n";
        if (!WriteFile(hStdInWrite, commands.c_str(), commands.length(), &written, NULL)) {
            std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
            return "";
        }
        CloseHandle(hStdInWrite);

        CHAR buffer[128];
        DWORD read;
        std::string output;
        while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &read, NULL) && read > 0) {
            buffer[read] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdOutRead);

        return ExtractLibAddress(output);
    }

    std::string ExecuteShellCommandNoSu(const std::string& command) {
        HANDLE hStdOutRead, hStdOutWrite;
        HANDLE hStdInRead, hStdInWrite;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
            std::cerr << "CreatePipe failed: " << GetLastError() << std::endl;
            return "";
        }

        if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "SetHandleInformation failed: " << GetLastError() << std::endl;
            return "";
        }

        if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
            std::cerr << "CreatePipe failed: " << GetLastError() << std::endl;
            return "";
        }

        if (!SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0)) {
            std::cerr << "SetHandleInformation failed: " << GetLastError() << std::endl;
            return "";
        }

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdError = hStdOutWrite;
        si.hStdOutput = hStdOutWrite;
        si.hStdInput = hStdInRead;
        si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        const std::string shellCmd = BuildHdAdbCommand(
            "shell \"getprop ro.secure ; /boot/android/android/system/xbin/bstk/su\"");
        std::vector<char> shellCmdBuf(shellCmd.begin(), shellCmd.end());
        shellCmdBuf.push_back('\0');

        if (!CreateProcessA(NULL, shellCmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            std::cerr << "CreateProcess failed: " << GetLastError() << std::endl;
            return "";
        }

        CloseHandle(hStdOutWrite);
        CloseHandle(hStdInRead);

        DWORD written;
        std::string commands = command + "\n";
        if (!WriteFile(hStdInWrite, commands.c_str(), commands.length(), &written, NULL)) {
            std::cerr << "WriteFile failed: " << GetLastError() << std::endl;
            return "";
        }
        CloseHandle(hStdInWrite);

        CHAR buffer[128];
        DWORD read;
        std::string output;
        while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &read, NULL) && read > 0) {
            buffer[read] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hStdOutRead);

        return ExtractLibAddress(output);
    }

    void ExecuteADBCommand(const std::string& command) {
        const std::string fullCommand = BuildHdAdbCommand("shell " + command);
        std::vector<char> cmdBuf(fullCommand.begin(), fullCommand.end());
        cmdBuf.push_back('\0');

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.wShowWindow = SW_HIDE;
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            std::cerr << "Failed to start process. Error: " << GetLastError() << std::endl;
            return;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    bool EnsureAdbConnection(const std::string& defaultPort) {
        g_activeAdbPort.clear();
        g_adbSerial.clear();

        RunHdAdbRaw("kill-server");

        if (!FindAndConnectAdbPort(defaultPort))
            return false;

        RunHdAdbRaw("devices");

        std::cerr << "[ADB] Using serial " << g_adbSerial << std::endl;
        return true;
    }

    std::string GetActiveAdbPort() {
        return g_activeAdbPort;
    }

    std::string GetAdbSerial() {
        return g_adbSerial;
    }
}
