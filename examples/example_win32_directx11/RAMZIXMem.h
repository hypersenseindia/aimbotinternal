#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <TlHelp32.h>
#include <tchar.h>
#define WIN32_LEAN_AND_MEAN
#include <winternl.h>
#include <sstream>
#include <windows.h>
#include <mmsystem.h>
#include <map>
#include <utility>
#include <mutex>

#include <future>
#include <thread>
#include <chrono>



#pragma comment(lib, "ntdll.lib")
#include <WtsApi32.h>
#pragma comment(lib, "wtsapi32.lib")



extern "C" NTSTATUS ZwReadVirtualMemory(HANDLE hProcess, LPVOID lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead = NULL);
extern "C" NTSTATUS ZwWriteVirtualMemory(HANDLE hProcess, LPVOID lpBaseAddress, void* lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead = NULL);
extern "C" NTSTATUS ZwProtectVirtualMemory(HANDLE hProcess, LPVOID BaseAddress, size_t  NumberOfBytesToProtect, ULONG NewAccessProtection, PULONG OldAccessProtection);

extern std::string MemoryLogs;
static bool botIgnoreOnly = false;
static bool fastinject = true;
static bool midinject = true;



class AimbotMemory
{

public:
    DWORD ProcessId = 0;
    HANDLE ProcessHandle = nullptr;
    HWND emulatorTargetWindow = nullptr;


    std::unordered_map<DWORD_PTR, int> originalValuesWrite;
    std::unordered_map<DWORD_PTR, int> originalValuesWrite2;
    std::unordered_map<DWORD_PTR, int> modifiedValuesWrite;
    std::unordered_map<DWORD_PTR, int> modifiedValuesWrite2;

    std::vector<DWORD_PTR> BlockedAddresses;

    std::unordered_map<DWORD_PTR, int> modifiedAoBs;
    std::mutex AimbotMemory::modifiedAoBsMutex;

    std::unordered_map<DWORD_PTR, std::vector<BYTE>> originalAoBs;
    std::unordered_map<DWORD_PTR, bool> modifiedAoBs2;
    //std::unordered_map<DWORD_PTR, int> modifiedAoBs;              // duplicated above?
    std::mutex mtx;

    std::map<DWORD_PTR, std::vector<BYTE>> someDataStructure;




    typedef struct _MEMORY_REGION {
        DWORD_PTR dwBaseAddr;
        DWORD_PTR dwMemorySize;
    }MEMORY_REGION;

    static int GetPid(const char* procname) {
        if (procname == NULL)
            return 0;

        wchar_t wProcName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, procname, -1, wProcName, MAX_PATH);

        DWORD pid = 0;
        DWORD threadCount = 0;

        DWORD level = 1;
        PWTS_PROCESS_INFO_EXW pWpiEx = NULL;
        DWORD count = 0;

        if (WTSEnumerateProcessesExW(WTS_CURRENT_SERVER_HANDLE, &level, WTS_ANY_SESSION, (LPWSTR*)&pWpiEx, &count)) {
            for (DWORD i = 0; i < count; i++) {
                if (pWpiEx[i].pProcessName != NULL && _wcsicmp(pWpiEx[i].pProcessName, wProcName) == 0) {
                    if (pWpiEx[i].NumberOfThreads > threadCount) {
                        threadCount = pWpiEx[i].NumberOfThreads;
                        pid = pWpiEx[i].ProcessId;
                    }
                }
            }
            WTSFreeMemoryExW((WTS_TYPE_CLASS)WTS_PROCESS_INFO_LEVEL_1, pWpiEx, count);
            if (pid != 0) return pid;
        }

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return 0;
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, wProcName) == 0) {
                    if ((int)pe.cntThreads > threadCount) {
                        threadCount = pe.cntThreads;
                        pid = pe.th32ProcessID;
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }

        CloseHandle(hSnap);
        if (pid != 0) return pid;

        // --- Window-based fallback for Emulators ---
        // Helpful if process enumeration is hooked or blocked (e.g., Anti-Debug).
        struct FindWindowData { const char* procName; DWORD pid; };
        FindWindowData data = { procname, 0 };

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            FindWindowData* pData = (FindWindowData*)lParam;
            wchar_t title[512];
            if (GetWindowTextW(hwnd, title, 512) > 0) {
                std::wstring wTitle(title);
                for (auto& c : wTitle) c = towlower(c); // Lowercase

                bool match = false;
                if (_stricmp(pData->procName, "HD-Player.exe") == 0 || _stricmp(pData->procName, "BlueStacks.exe") == 0) {
                    if (wTitle.find(L"bluestacks") != std::wstring::npos) match = true;
                }
                else if (_stricmp(pData->procName, "dnplayer.exe") == 0 || _stricmp(pData->procName, "LdBoxHeadless.exe") == 0) {
                    if (wTitle.find(L"ldplayer") != std::wstring::npos) match = true;
                }
                else if (_stricmp(pData->procName, "Nox.exe") == 0 || _stricmp(pData->procName, "NoxVMHandle.exe") == 0) {
                    if (wTitle.find(L"nox") != std::wstring::npos) match = true;
                }
                else if (_stricmp(pData->procName, "MEmu.exe") == 0 || _stricmp(pData->procName, "MEmuHeadless.exe") == 0) {
                    if (wTitle.find(L"memu") != std::wstring::npos) match = true;
                }
                else if (_stricmp(pData->procName, "AndroidEmulator.exe") == 0 || _stricmp(pData->procName, "Gameloop.exe") == 0) {
                    if (wTitle.find(L"gameloop") != std::wstring::npos || wTitle.find(L"turbo aow") != std::wstring::npos) match = true;
                }
                else if (_stricmp(pData->procName, "MSIAppPlayer.exe") == 0) {
                    if (wTitle.find(L"msi") != std::wstring::npos) match = true;
                }

                if (match) {
                    GetWindowThreadProcessId(hwnd, &pData->pid);
                    return FALSE; // Found
                }
            }
            return TRUE;
            }, (LPARAM)&data);

        if (data.pid != 0) return data.pid;

        return 0;
    }

    const char* GetEmulatorRunning() {
        // Keep this list broad: emulator updates frequently change process names.
        // Return the first match found.
        static const char* kCandidates[] = {
            // BlueStacks / MSI App Player (BS5 engine)
            "HD-Player.exe",
            "BlueStacks.exe",
            "Bluestacks.exe",
            "MSIAppPlayer.exe",
            "BlueStacks_nxt.exe",
            "BstkSVC.exe",

            // LDPlayer (variants)
            "LdVBoxHeadless.exe",
            "Ld9BoxHeadless.exe",
            "LdBoxHeadless.exe",
            "dnplayer.exe",

            // MEmu
            "MEmuHeadless.exe",
            "MEmu.exe",

            // Nox
            "Nox.exe",
            "NoxVMHandle.exe",

            // GameLoop / Tencent
            "AndroidEmulatorEn.exe",
            "AndroidEmulator.exe",
            "Gameloop.exe",
            "AppMarket.exe",

            // Generic / older wrappers
            "AndroidProcess.exe",
            "aow_exe.exe",
        };

        for (const char* exe : kCandidates) {
            if (exe && *exe && GetPid(exe) != 0)
                return exe;
        }

        // --- Window-based fallback for GetEmulatorRunning ---
        struct FindWindowData { const char* foundExe; };
        FindWindowData data = { nullptr };

        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            FindWindowData* pData = (FindWindowData*)lParam;
            wchar_t title[512];
            if (GetWindowTextW(hwnd, title, 512) > 0) {
                std::wstring wTitle(title);
                for (auto& c : wTitle) c = towlower(c); // Lowercase

                struct WindowMatch { const wchar_t* title_sub; const char* exe; };
                static WindowMatch matches[] = {
                    { L"bluestacks", "HD-Player.exe" },
                    { L"msi app player", "HD-Player.exe" },
                    { L"msi center", "HD-Player.exe" },
                    { L"ldplayer", "dnplayer.exe" },
                    { L"nox", "Nox.exe" },
                    { L"memu", "MEmu.exe" },
                    { L"gameloop", "AndroidEmulator.exe" },
                    { L"turbo aow", "AndroidEmulator.exe" }
                };

                for (auto& m : matches) {
                    if (wTitle.find(m.title_sub) != std::wstring::npos) {
                        pData->foundExe = m.exe;
                        return FALSE; // Found
                    }
                }
            }
            return TRUE;
            }, (LPARAM)&data);

        if (data.foundExe != nullptr) return data.foundExe;

        return nullptr;
    }


    std::vector<DWORD_PTR> AddressScan;

    std::vector<BYTE> ScanAimbot = { 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x43 };

    


    void HeadTracking()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {

            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "AimFov : Applying";


        bool st = ReplacePattern(0x0000000000000000, 0x00007fffffffffff,
            new BYTE[20]{ 0xF0, 0x8F, 0xBD, 0xE8, 0xA4, 0x70, 0x7D, 0x3F, 0x3A, 0xCD, 0x13, 0x3F, 0x0A, 0xD7, 0x23, 0x3C, 0xBD, 0x37, 0x86, 0x35 },
            new BYTE[20]{ 0xF0, 0x8F, 0xBD, 0xE8, 0xA4, 0x70, 0x7D, 0x3F, 0x3A, 0xCD, 0x13, 0x3F, 0x0A, 0xD7, 0x23, 0x3C, 0xBD, 0x37, 0x86, 0xB5 },
            true);



        if (st)
        {

            MemoryLogs = "AimFov : Successfully Injected!";


        }

        else
        {
            MemoryLogs = "AimFov : Failed To Apply!";


        }

        // CloseHandle(ProcessHandle);
    }


    void HeadTrackingOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {

            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "AimFov : Applying";


        bool st = ReplacePattern(0x0000000000000000, 0x00007fffffffffff,
            new BYTE[20]{ 0xF0, 0x8F, 0xBD, 0xE8, 0xA4, 0x70, 0x7D, 0x3F, 0x3A, 0xCD, 0x13, 0x3F, 0x0A, 0xD7, 0x23, 0x3C, 0xBD, 0x37, 0x86, 0xB5 },
            new BYTE[20]{ 0xF0, 0x8F, 0xBD, 0xE8, 0xA4, 0x70, 0x7D, 0x3F, 0x3A, 0xCD, 0x13, 0x3F, 0x0A, 0xD7, 0x23, 0x3C, 0xBD, 0x37, 0x86, 0x35 },
            true);



        if (st)
        {

            MemoryLogs = "AimFov : Successfully Injected!";


        }

        else
        {
            MemoryLogs = "AimFov : Failed To Apply!";


        }

        // CloseHandle(ProcessHandle);
    }




    void SniperSwitchon()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Sniper Switch : Applying";

        bool st = ReplacePattern(0x0000000000000000, 0x00007fffffffffff,
            new BYTE[45]{ 0x3F, 0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x34, 0x42, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F },
            new BYTE[30]{ 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x34, 0x42, 0x01, 0x00, 0x00, 0x00, 0x01 },
            true);

        if (st) { MemoryLogs = "Sniper Switch : Successfully Injected!"; }
        else { MemoryLogs = "Sniper Switch : Failed To Apply!"; }

        // CloseHandle(ProcessHandle);
    }

    void SniperSwitchoff()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Sniper Switch : Removing";

        bool st = ReplacePattern(0x0000000000000000, 0x00007fffffffffff,
            new BYTE[30]{ 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x34, 0x42, 0x01, 0x00, 0x00, 0x00, 0x01 },
            new BYTE[45]{ 0x3F, 0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x20, 0x41, 0x00, 0x00, 0x34, 0x42, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F },
            true);

        if (st) { MemoryLogs = "Sniper Switch : Successfully Removed!"; }
        else { MemoryLogs = "Sniper Switch : Failed To Remove!"; }

        // CloseHandle(ProcessHandle);
    }

    void SniperDelayFixON()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Sniper Delay Fix : Applying";

        // Use full-length reversible pattern to avoid broad accidental replacements.
        bool st = ReplacePattern(0x0000000000000000, 0x00007fffffffffff,
            new BYTE[21]{ 0xEA, 0x00, 0x60, 0xA0, 0xE3, 0x06, 0x00, 0xA0, 0xE1, 0x18, 0xD0, 0x8D, 0xE2, 0xF0, 0x87, 0xBD, 0xE8, 0x66, 0x2B, 0x70, 0x05 },
            new BYTE[21]{ 0x01, 0x00, 0xAF, 0xA0, 0xE3, 0x06, 0x00, 0xA0, 0xE1, 0x18, 0xD0, 0x8D, 0xE2, 0xF0, 0x87, 0xBD, 0xE8, 0x66, 0x2B, 0x70, 0x05 },
            true);

        if (st)
        {
            MemoryLogs = "Sniper Delay Fix : Successfully Injected!";
        }
        else
        {
            MemoryLogs = "Sniper Delay Fix : Failed To Apply!";
        }

        // CloseHandle(ProcessHandle);
    }

    void SniperDelayFixOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Sniper Delay Fix : Removing";

        // Restore only previously patched bytes (full signature), not generic short sequences.
        bool st = ReplacePattern(0x0000000000000000, 0x00007fffffffffff,
            new BYTE[20]{ 0xEA, 0x00, 0x60, 0xA0, 0xF3, 0x06, 0x00, 0xA0, 0xE1, 0x18, 0xD0, 0x4B, 0xE2, 0x02, 0x8B, 0xBD, 0xEC, 0x70, 0x8C },
            new BYTE[20]{ 0xEA, 0x00, 0x60, 0xA0, 0xE3, 0x06, 0x00, 0xA0, 0xE1, 0x18, 0xD0, 0x4B, 0xE2, 0x02, 0x8B, 0xBD, 0xEC, 0x70, 0x8C },
            true);

        if (st)
        {
            MemoryLogs = "Sniper Delay Fix : Successfully Removed!";
        }
        else
        {
            MemoryLogs = "Sniper Delay Fix : Failed To Remove!";
        }

        // CloseHandle(ProcessHandle);
    }

    std::vector<DWORD_PTR> NewCameraRight;

    bool SaveCameraRightAoB()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return false;
        }

        MemoryLogs = "Camera Right Scan: Scanning!";

        NewCameraRight.clear();

        BYTE* SearchPattern = new BYTE[88]{ 0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0xFF };

        if (fastinject)
        {
            MidFindPattern2(0x0000000000000000, 0x00007fffffffffff, SearchPattern, NewCameraRight);
        }
        else
        {
            SlowFindPattern(0x0000000000000000, 0x00007fffffffffff, SearchPattern, NewCameraRight);
        }

        if (NewCameraRight.empty())
        {
            MemoryLogs = "Camera Right Pattern Not Found!";
            return false;
        }

        MemoryLogs = "Camera Right : Loaded Successfully! Scan Results Found";
        return true;
    }

    bool ActivateCameraRight()
    {
        if (NewCameraRight.empty()) {
            MemoryLogs = "Camera Right Load: Address Not Found!";
            return false;
        }

        MemoryLogs = "Camera Right: Scanning...";

        BYTE* ReplacePattern = new BYTE[88]{ 0x00,0x00,0x00,0x00,0x00,0x80,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0xFF };
        int RepByteSize = _msize(ReplacePattern);

        DWORD OldProtect;
        for (int i = 0; i < (int)NewCameraRight.size(); i++)
        {
            ChangeProtection(NewCameraRight[i], RepByteSize, PAGE_EXECUTE_READWRITE, OldProtect);
            WriteProcessMemory(ProcessHandle, (LPVOID)NewCameraRight[i], ReplacePattern, RepByteSize, 0);
        }

        MemoryLogs = "Camera Right : Activated Successfully!";
        return true;
    }

    bool OFFCameraRight()
    {
        if (NewCameraRight.empty()) {
            MemoryLogs = "Camera Right Load: Address Not Found!";
            return false;
        }

        MemoryLogs = "Camera Right: Scanning...";

        BYTE* ReplacePattern = new BYTE[88]{ 0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0xBF,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0x7F,0x00,0x00,0x80,0xFF };
        int RepByteSize = _msize(ReplacePattern);

        DWORD OldProtect;
        for (int i = 0; i < (int)NewCameraRight.size(); i++)
        {
            ChangeProtection(NewCameraRight[i], RepByteSize, PAGE_EXECUTE_READWRITE, OldProtect);
            WriteProcessMemory(ProcessHandle, (LPVOID)NewCameraRight[i], ReplacePattern, RepByteSize, 0);
        }

        MemoryLogs = "Camera Right : Disabled Successfully!";
        return true;
    }


    void BlackSkyON()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Black Sky : Applying";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (original)
            new BYTE[16]{
            0xA4, 0x70, 0x7D, 0x3F,
            0x3A, 0xCD, 0x13, 0x3F,
            0x0A, 0xD7, 0x23, 0x3C,
            0xBD, 0x37, 0x86, 0x35 },

            // REPLACE (black sky)
            new BYTE[16]{
            0xA4, 0x70, 0x7D, 0x3F,
            0x3A, 0xCD, 0x13, 0x3F,
            0x0A, 0xD7, 0x23, 0x3C,
            0x00, 0x00, 0x80, 0xBF },

            true);

        if (st)
            MemoryLogs = "Black Sky : Successfully Injected!";
        else
            MemoryLogs = "Black Sky : Failed To Apply!";

        // CloseHandle(ProcessHandle);
    }



    void BlackSkyOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Black Sky : Removing";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (modified)
            new BYTE[16]{
            0xA4, 0x70, 0x7D, 0x3F,
            0x3A, 0xCD, 0x13, 0x3F,
            0x0A, 0xD7, 0x23, 0x3C,
            0x00, 0x00, 0x80, 0xBF },

            // REPLACE (restore original)
            new BYTE[16]{
            0xA4, 0x70, 0x7D, 0x3F,
            0x3A, 0xCD, 0x13, 0x3F,
            0x0A, 0xD7, 0x23, 0x3C,
            0xBD, 0x37, 0x86, 0x35 },

            true);

        if (st)
            MemoryLogs = "Black Sky : Successfully Removed!";
        else
            MemoryLogs = "Black Sky : Failed To Remove!";

        // CloseHandle(ProcessHandle);
    }

    void GlitchFireON()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Glitch Fire : Applying";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (original)
            new BYTE[34]{
            0xC0, 0x41, 0x00, 0x00, 0x10, 0xC1, 0x00, 0x00, 0x90, 0xC1, 0x00, 0x00, 0x70, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F },

            // REPLACE (glitch fire)
            new BYTE[34]{
            0xC0, 0x41, 0x00, 0x00, 0x10, 0xC1, 0x00, 0x00, 0x90, 0xC1, 0x00, 0x00, 0x70, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F },

            true);

        if (st)
            MemoryLogs = "Glitch Fire : Successfully Injected!";
        else
            MemoryLogs = "Glitch Fire : Failed To Apply!";

        // CloseHandle(ProcessHandle);
    }



    void GlitchFireOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Glitch Fire : Removing";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (glitch)
            new BYTE[34]{
            0xC0, 0x41, 0x00, 0x00, 0x10, 0xC1, 0x00, 0x00, 0x90, 0xC1, 0x00, 0x00, 0x70, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F },

            // REPLACE (restore original)
            new BYTE[34]{
            0xC0, 0x41, 0x00, 0x00, 0x10, 0xC1, 0x00, 0x00, 0x90, 0xC1, 0x00, 0x00, 0x70, 0x41, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F },

            true);

        if (st)
            MemoryLogs = "Glitch Fire : Successfully Removed!";
        else
            MemoryLogs = "Glitch Fire : Failed To Remove!";

        // CloseHandle(ProcessHandle);
    }


    void FastReloadON()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Fast Reload : Applying";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (original)
            new BYTE[48]{
            0x30, 0x48, 0xBD, 0xE8, 0x05, 0x54, 0xDA, 0xEA,
            0x04, 0x00, 0xA0, 0xE1, 0x0A, 0x00, 0x00, 0xEB,
            0x00, 0x50, 0xA0, 0xE1, 0x04, 0x00, 0xA0, 0xE1,
            0x99, 0x10, 0xA0, 0xE3, 0x6D, 0x00, 0x00, 0xEB,
            0x00, 0x0A, 0xB7, 0xEE, 0x10, 0x0A, 0x01, 0xEE,
            0x00, 0x0A, 0x31, 0xEE, 0x10, 0x5A, 0x01, 0xEE },

            // REPLACE (fast reload)
            new BYTE[48]{
            0x30, 0x48, 0xBD, 0xE8, 0x05, 0x54, 0xDA, 0xEA,
            0x04, 0x00, 0xA0, 0xE1, 0x0A, 0x00, 0x00, 0xEB,
            0x00, 0x50, 0xA0, 0xE1, 0x04, 0x00, 0xA0, 0xE1,
            0x99, 0x10, 0xA0, 0xE3, 0x00, 0x00, 0x00, 0xEB,
            0x00, 0x0A, 0xB7, 0xEE, 0x10, 0x0A, 0x01, 0xEE,
            0x00, 0x0A, 0x31, 0xEE, 0x10, 0x5A, 0x01, 0xEE },

            true);

        if (st)
            MemoryLogs = "Fast Reload : Successfully Injected!";
        else
            MemoryLogs = "Fast Reload : Failed To Apply!";

        // CloseHandle(ProcessHandle);
    }



    void FastReloadOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Fast Reload : Removing";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (modified)
            new BYTE[48]{
            0x30, 0x48, 0xBD, 0xE8, 0x05, 0x54, 0xDA, 0xEA,
            0x04, 0x00, 0xA0, 0xE1, 0x0A, 0x00, 0x00, 0xEB,
            0x00, 0x50, 0xA0, 0xE1, 0x04, 0x00, 0xA0, 0xE1,
            0x99, 0x10, 0xA0, 0xE3, 0x00, 0x00, 0x00, 0xEB,
            0x00, 0x0A, 0xB7, 0xEE, 0x10, 0x0A, 0x01, 0xEE,
            0x00, 0x0A, 0x31, 0xEE, 0x10, 0x5A, 0x01, 0xEE },

            // REPLACE (restore original)
            new BYTE[48]{
            0x30, 0x48, 0xBD, 0xE8, 0x05, 0x54, 0xDA, 0xEA,
            0x04, 0x00, 0xA0, 0xE1, 0x0A, 0x00, 0x00, 0xEB,
            0x00, 0x50, 0xA0, 0xE1, 0x04, 0x00, 0xA0, 0xE1,
            0x99, 0x10, 0xA0, 0xE3, 0x6D, 0x00, 0x00, 0xEB,
            0x00, 0x0A, 0xB7, 0xEE, 0x10, 0x0A, 0x01, 0xEE,
            0x00, 0x0A, 0x31, 0xEE, 0x10, 0x5A, 0x01, 0xEE },

            true);

        if (st)
            MemoryLogs = "Fast Reload : Successfully Removed!";
        else
            MemoryLogs = "Fast Reload : Failed To Remove!";

        // CloseHandle(ProcessHandle);
    }

    void OnlyRedON()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Only Red : Applying";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (original)
            new BYTE[17]{
            0x3F, 0xE6, 0x00, 0x00,
            0x00, 0x00, 0x00, 0xB0,
            0x40, 0x00, 0x00, 0x80,
            0x3F, 0x00, 0x00, 0x40,
            0x3F },

            // REPLACE (only red)
            new BYTE[17]{
            0x3F, 0xE6, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x0A,
            0x5F, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00 },

            true);

        if (st)
            MemoryLogs = "Only Red : Successfully Injected!";
        else
            MemoryLogs = "Only Red : Failed To Apply!";

        // CloseHandle(ProcessHandle);
    }



    void OnlyRedOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Only Red : Removing";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (modified)
            new BYTE[17]{
            0x3F, 0xE6, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x0A,
            0x5F, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00 },

            // REPLACE (restore original)
            new BYTE[17]{
            0x3F, 0xE6, 0x00, 0x00,
            0x00, 0x00, 0x00, 0xB0,
            0x40, 0x00, 0x00, 0x80,
            0x3F, 0x00, 0x00, 0x40,
            0x3F },

            true);

        if (st)
            MemoryLogs = "Only Red : Successfully Removed!";
        else
            MemoryLogs = "Only Red : Failed To Remove!";

        // CloseHandle(ProcessHandle);
    }


    void GuestResetON()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Guest Reset : Applying";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (original — 36 bytes)
            new BYTE[36]{
                0x10, 0x40, 0x2D, 0xE9, 0xD0, 0x40, 0x9F, 0xE5, 0x04, 0x40, 0x8F, 0xE0,
                0x00, 0x00, 0xD4, 0xE5, 0x00, 0x00, 0x50, 0xE3, 0x04, 0x00, 0x00, 0x1A,
                0xC0, 0x00, 0x9F, 0xE5, 0x00, 0x00, 0x9F, 0xE7, 0xA9, 0x36, 0xA9, 0xEB },

            // REPLACE (guest reset — MOV r0,#1; BX lr + tail)
            new BYTE[36]{
                0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1, 0x04, 0x40, 0x8F, 0xE0,
                0x00, 0x00, 0xD4, 0xE5, 0x00, 0x00, 0x50, 0xE3, 0x04, 0x00, 0x00, 0x1A,
                0xC0, 0x00, 0x9F, 0xE5, 0x00, 0x00, 0x9F, 0xE7, 0xA9, 0x36, 0xA9, 0xEB },

            true);

        if (st)
            MemoryLogs = "Guest Reset : Successfully Injected!";
        else
            MemoryLogs = "Guest Reset : Failed To Apply!";

        // CloseHandle(ProcessHandle);
    }



    void GuestResetOFF()
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return;
        }

        MemoryLogs = "Guest Reset : Removing";

        bool st = ReplacePattern(
            0x0000000000000000,
            0x00007fffffffffff,

            // SEARCH (modified — guest reset patch)
            new BYTE[36]{
                0x01, 0x00, 0xA0, 0xE3, 0x1E, 0xFF, 0x2F, 0xE1, 0x04, 0x40, 0x8F, 0xE0,
                0x00, 0x00, 0xD4, 0xE5, 0x00, 0x00, 0x50, 0xE3, 0x04, 0x00, 0x00, 0x1A,
                0xC0, 0x00, 0x9F, 0xE5, 0x00, 0x00, 0x9F, 0xE7, 0xA9, 0x36, 0xA9, 0xEB },

            // REPLACE (restore original)
            new BYTE[36]{
                0x10, 0x40, 0x2D, 0xE9, 0xD0, 0x40, 0x9F, 0xE5, 0x04, 0x40, 0x8F, 0xE0,
                0x00, 0x00, 0xD4, 0xE5, 0x00, 0x00, 0x50, 0xE3, 0x04, 0x00, 0x00, 0x1A,
                0xC0, 0x00, 0x9F, 0xE5, 0x00, 0x00, 0x9F, 0xE7, 0xA9, 0x36, 0xA9, 0xEB },

            true);

        if (st)
            MemoryLogs = "Guest Reset : Successfully Removed!";
        else
            MemoryLogs = "Guest Reset : Failed To Remove!";

        // CloseHandle(ProcessHandle);
    }



    template<typename T>
    bool SafeReadProcessMemory(DWORD_PTR address, T& value)
    {
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(
            hProcess,
            reinterpret_cast<LPCVOID>(address),
            &value,
            sizeof(T),
            &bytesRead
        ) && bytesRead == sizeof(T);
    }


    template<typename T>
    bool SafeWriteProcessMemory(DWORD_PTR address, const T& value)
    {
        SIZE_T bytesWritten = 0;
        return WriteProcessMemory(
            hProcess,
            reinterpret_cast<LPVOID>(address),
            &value,
            sizeof(T),
            &bytesWritten
        ) && bytesWritten == sizeof(T);
    }


    bool SafeReadProcessMemory(DWORD_PTR address, int& buffer) {
        if (ReadProcessMemory(ProcessHandle, (LPVOID)address, &buffer, sizeof(buffer), NULL)) {
            return true;
        }
        return false; // Return false if read fails
    }

    // Safe memory write function with error handling
    bool SafeWriteProcessMemory(DWORD_PTR address, int& buffer) {
        if (WriteProcessMemory(ProcessHandle, (LPVOID)address, &buffer, sizeof(buffer), 0)) {
            return true;
        }
        return false; // Return false if write fails
    }

    BOOL BestReadMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead) {
        static auto pReadRenderedMemory = reinterpret_cast<decltype(&ReadProcessMemory)>(
            GetProcAddress(GetModuleHandleW((L"kernel32.dll")), ("ReadProcessMemory"))
            );

        if (!pReadRenderedMemory) {
            return FALSE;
        }

        return pReadRenderedMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesRead);
    }


    BOOL BestWriteMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesWritten) {
        static auto pWriteProcessMemory = reinterpret_cast<decltype(&WriteProcessMemory)>(
            GetProcAddress(GetModuleHandleW((L"kernel32.dll")), ("WriteProcessMemory"))
            );

        if (!pWriteProcessMemory) {
            return FALSE;
        }

        return pWriteProcessMemory(hProcess, lpBaseAddress, lpBuffer, nSize, lpNumberOfBytesWritten);
    }


    bool MonitorMemory(HANDLE ProcessHandle, DWORD_PTR monitorAddress, int& lastValue, int& currentValue)
    {

        if (ReadProcessMemory(ProcessHandle, reinterpret_cast<LPCVOID>(monitorAddress), &currentValue, sizeof(currentValue), NULL))
        {

            if (currentValue != lastValue)
            {
                lastValue = currentValue;
                return true;
            }
        }
        else
        {
            std::cerr << "Failed to read memory at address: " << std::hex << monitorAddress << std::dec << std::endl;
        }
        return false;
    }







    


    void Reapply() {
        if (!AttackProcess(GetEmulatorRunning())) {

            std::cout << "Failed to attach to process for reapplying changes!";
            return;
        }

        for (const auto& entry : modifiedValuesWrite) {
            DWORD_PTR address = entry.first;
            int valueToReapply = entry.second;


            WriteProcessMemory(ProcessHandle, (LPVOID)address, &valueToReapply, sizeof(valueToReapply), NULL);
        }

        for (const auto& entry : modifiedValuesWrite2) {
            DWORD_PTR address = entry.first;
            int valueToReapply = entry.second;


            WriteProcessMemory(ProcessHandle, (LPVOID)address, &valueToReapply, sizeof(valueToReapply), NULL);
        }

        MemoryLogs = "Aimbot Has Been Reactivate";
        // CloseHandle(ProcessHandle);
    }




    BYTE* ReadBytes(DWORD_PTR address, int size)
    {
        BYTE* buffer = new BYTE[size];
        SIZE_T bytesRead = 0;

        BOOL readSuccess = ReadProcessMemory(ProcessHandle, (LPCVOID)address, buffer, size, &bytesRead);

        if (readSuccess && bytesRead == size)
        {
            return buffer;
        }
        else
        {
            delete[] buffer;
            return nullptr;
        }
    }

    bool Replace(DWORD_PTR address, BYTE* replacePattern)
    {
        return WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replacePattern, _msize(replacePattern), nullptr);
    }



    void ReWrite(std::string type, DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* Search, BYTE* Replace)
    {
        if (!AttackProcess(GetEmulatorRunning()))
            MemoryLogs = type + ": An unexpected error occurred";

        MemoryLogs = "Applying - " + type;
        bool Status = ReplacePattern(dwStartRange, dwEndRange, Search, Replace);
        if (Status)
            MemoryLogs = type + " - Enabled!";
        else
            MemoryLogs = type + " : Failed to Enable!";

        // CloseHandle(ProcessHandle);
    }
    void deWrite(std::string type, DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* Search, BYTE* Replace)
    {
        if (!AttackProcess(GetEmulatorRunning()))
            MemoryLogs = type + ": An unexpected error occurred";;

        bool Status = ReplacePattern(dwStartRange, dwEndRange, Search, Replace);
        if (Status)
            MemoryLogs = type + " - Disabled!";
        else
            MemoryLogs = type + " : Failed to Disable!";

        // CloseHandle(ProcessHandle);
    }

    static bool EnableDebugPrivilege() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        TOKEN_PRIVILEGES tp{};
        LUID luid{};
        if (LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        }
        CloseHandle(hToken);
        return true;
    }

    static bool IsKnownEmulatorExe(const char* exeName) {
        if (!exeName || !*exeName)
            return false;
        static const char* kKnown[] = {
            "HD-Player.exe", "BlueStacks.exe", "Bluestacks.exe", "MSIAppPlayer.exe",
            "BlueStacks_nxt.exe", "BstkSVC.exe", "LdVBoxHeadless.exe", "Ld9BoxHeadless.exe",
            "LdBoxHeadless.exe", "dnplayer.exe", "MEmuHeadless.exe", "MEmu.exe",
            "Nox.exe", "NoxVMHandle.exe", "AndroidEmulatorEn.exe", "AndroidEmulator.exe",
            "Gameloop.exe", "AppMarket.exe", "AndroidProcess.exe", "aow_exe.exe"
        };
        for (const char* known : kKnown) {
            if (_stricmp(exeName, known) == 0)
                return true;
        }
        return false;
    }

    static std::string GetProcessExeName(DWORD pid) {
        if (pid == 0)
            return {};
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
            return {};
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        std::string result;
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == pid) {
                    char narrow[MAX_PATH]{};
                    WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, narrow, MAX_PATH, nullptr, nullptr);
                    result = narrow;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return result;
    }

    DWORD ResolveEmulatorProcessId() {
        EnableDebugPrivilege();

        static const char* kCandidates[] = {
            "HD-Player.exe", "BlueStacks.exe", "Bluestacks.exe", "MSIAppPlayer.exe",
            "BlueStacks_nxt.exe", "dnplayer.exe", "LdBoxHeadless.exe", "Nox.exe",
            "MEmu.exe", "AndroidEmulator.exe", "Gameloop.exe"
        };

        for (const char* exe : kCandidates) {
            const DWORD pid = GetPid(exe);
            if (pid == 0)
                continue;

            const DWORD accessList[] = {
                PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
                PROCESS_ALL_ACCESS
            };

            for (DWORD access : accessList) {
                HANDLE probe = OpenProcess(access, FALSE, pid);
                if (probe) {
                    CloseHandle(probe);
                    return pid;
                }
            }
        }
        return 0;
    }

    void SetEmulatorTargetWindow(HWND hwnd) {
        emulatorTargetWindow = hwnd;
    }

    BOOL AttackProcessByPid(DWORD procId, const char* label = "emulator") {
        if (procId == 0) {
            MemoryLogs = std::string("AttackProcess: invalid PID for ") + label;
            return FALSE;
        }

        DWORD exitCode = 0;
        if (ProcessHandle && ProcessId == procId &&
            GetExitCodeProcess(ProcessHandle, &exitCode) && exitCode == STILL_ACTIVE) {
            return TRUE;
        }

        if (ProcessHandle) {
            CloseHandle(ProcessHandle);
            ProcessHandle = nullptr;
        }

        ProcessId = procId;
        EnableDebugPrivilege();

        const DWORD accessList[] = {
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            PROCESS_ALL_ACCESS
        };

        for (DWORD access : accessList) {
            ProcessHandle = OpenProcess(access, FALSE, ProcessId);
            if (ProcessHandle)
                return TRUE;
        }

        const DWORD err = GetLastError();
        MemoryLogs = std::string("AttackProcess: OpenProcess failed (") + std::to_string(err) + "): " + label;
        return FALSE;
    }

    static DWORD GetPidFromWindow(HWND hwnd) {
        if (!hwnd || !IsWindow(hwnd))
            return 0;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        return pid;
    }

    BOOL AttackProcess(const char* procname) {
        DWORD exitCode = 0;
        if (ProcessHandle && GetExitCodeProcess(ProcessHandle, &exitCode) && exitCode == STILL_ACTIVE) {
            return TRUE;
        }
        else if (ProcessHandle) {
            CloseHandle(ProcessHandle);
            ProcessHandle = nullptr;
        }

        const char* target = (procname && *procname) ? procname : GetEmulatorRunning();
        DWORD procId = 0;

        if (target && *target)
            procId = GetPid(target);

        if (procId == 0)
            procId = ResolveEmulatorProcessId();

        if (procId == 0 && emulatorTargetWindow) {
            const DWORD windowPid = GetPidFromWindow(emulatorTargetWindow);
            const std::string windowExe = GetProcessExeName(windowPid);
            if (IsKnownEmulatorExe(windowExe.c_str()))
                procId = windowPid;
        }

        if (procId == 0) {
            MemoryLogs = target && *target
                ? std::string("AttackProcess: GetPid returned 0 for ") + target
                : std::string("AttackProcess: emulator process not found");
            return FALSE;
        }

        const char* attachLabel = (target && *target) ? target : GetProcessExeName(procId).c_str();
        if (!attachLabel || !*attachLabel)
            attachLabel = "emulator";
        return AttackProcessByPid(procId, attachLabel);
    }
    bool ChangeProtection(ULONG Address, size_t size, DWORD NewProtect, DWORD& OldProtect)
    {
        return VirtualProtectEx(ProcessHandle, (LPVOID)Address, size, NewProtect, &OldProtect);;
    }

    bool ReplacePattern(DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* SearchAob, BYTE* ReplaceAob, bool ForceWrite = false)
    {
        int RepByteSize = _msize(ReplaceAob);
        if (RepByteSize <= 0) return false;
        std::vector<DWORD_PTR> foundedAddress;


        if (fastinject)
        {
            MidFindPattern2(dwStartRange, dwEndRange, SearchAob, foundedAddress);

        }
        else
        {
            // Slow, but safe search
            SlowFindPattern(dwStartRange, dwEndRange, SearchAob, foundedAddress);
        }
        if (foundedAddress.empty())
            return false;

        OutputDebugStringA(std::to_string(foundedAddress.size()).c_str());

        DWORD OldProtect;
        for (int i = 0; i < foundedAddress.size(); i++)
        {
            ChangeProtection(foundedAddress[i], RepByteSize, PAGE_EXECUTE_READWRITE, OldProtect);
            WriteProcessMemory(ProcessHandle, (LPVOID)foundedAddress[i], ReplaceAob, RepByteSize, 0);
        }

        return true;
    }

    // Find full AoB once, then write only the listed bytes (avoids replacing huge buffers).
    bool PatchBytesAtOffsetsForPattern(DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* searchAob,
        const std::pair<size_t, BYTE>* patches, size_t patchCount)
    {
        if (!searchAob || !patches || patchCount == 0)
            return false;
        const SIZE_T searchLen = _msize(searchAob);
        if (searchLen == 0)
            return false;
        for (size_t i = 0; i < patchCount; ++i) {
            if (patches[i].first >= searchLen)
                return false;
        }

        std::vector<DWORD_PTR> foundedAddress;
        if (fastinject)
            MidFindPattern2(dwStartRange, dwEndRange, searchAob, foundedAddress);
        else
            SlowFindPattern(dwStartRange, dwEndRange, searchAob, foundedAddress);
        if (foundedAddress.empty())
            return false;

        for (DWORD_PTR base : foundedAddress) {
            for (size_t i = 0; i < patchCount; ++i) {
                const size_t off = patches[i].first;
                BYTE b = static_cast<BYTE>(patches[i].second);
                DWORD OldProtect = 0;
                ChangeProtection(static_cast<ULONG>(base + off), 1, PAGE_EXECUTE_READWRITE, OldProtect);
                WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(base + off), &b, 1, nullptr);
            }
        }
        return true;
    }

    bool HookPattern(DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* SearchAob, BYTE* ReplaceAob, std::vector<DWORD_PTR>& AddressRet) {
        if (!AttackProcess(GetEmulatorRunning())) return false;
        int RepByteSize = _msize(ReplaceAob);
        if (RepByteSize <= 0) return false;

        if (AddressRet.empty()) {

            if (fastinject == true) {

                FastFindPattern(dwStartRange, dwEndRange, SearchAob, AddressRet);
            }
            else {

                SlowFindPattern(dwStartRange, dwEndRange, SearchAob, AddressRet);
            }

            if (AddressRet.empty()) return false;

            DWORD OldProtect;
            for (int i = 0; i < AddressRet.size(); i++)
            {
                WriteProcessMemory(ProcessHandle, (LPVOID)AddressRet[i], ReplaceAob, RepByteSize, 0);
            }

            return true;
        }
        else {
            DWORD OldProtect;
            for (int i = 0; i < AddressRet.size(); i++)
            {
                WriteProcessMemory(ProcessHandle, (LPVOID)AddressRet[i], ReplaceAob, RepByteSize, 0);
            }
            return true;
        }
        // CloseHandle(ProcessHandle);
    }

    bool MidFindPattern2(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet)
    {
        if (!SearchBytes) return false;
        const SIZE_T nSearchSize = _msize(SearchBytes);

        MEMORY_BASIC_INFORMATION mbi;
        DWORD_PTR dwAddress = StartRange;
        std::vector<MEMORY_REGION> memoryRegions;

        // collect memory regions
        while (VirtualQueryEx(ProcessHandle, (LPCVOID)dwAddress, &mbi, sizeof(mbi)) &&
            dwAddress < EndRange &&
            (dwAddress + mbi.RegionSize > dwAddress))
        {
            if (mbi.State == MEM_COMMIT &&
                !(mbi.Protect & PAGE_GUARD) &&
                mbi.Protect != PAGE_NOACCESS &&
                !(mbi.AllocationProtect & PAGE_NOCACHE))
            {
                memoryRegions.push_back({ (DWORD_PTR)mbi.BaseAddress, mbi.RegionSize });
            }
            dwAddress = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
        }

        if (memoryRegions.empty()) return false;


        const size_t MAX_THREADS = 6;
        size_t workerCount = (std::min)(memoryRegions.size(), MAX_THREADS);

        if (workerCount == 0) workerCount = 1;

        std::vector<std::vector<MEMORY_REGION>> workerRegions(workerCount);
        for (size_t i = 0; i < memoryRegions.size(); ++i)
            workerRegions[i % workerCount].push_back(memoryRegions[i]);

        std::vector<std::vector<DWORD_PTR>> workerResults(workerCount);
        std::vector<std::thread> workers;

        auto workerProc = [&](size_t idx)
            {
                auto& regs = workerRegions[idx];
                auto& out = workerResults[idx];

                for (const auto& r : regs)
                {
                    if (r.dwMemorySize == 0) continue;

                    std::unique_ptr<BYTE[]> buffer(new BYTE[r.dwMemorySize]);
                    SIZE_T bytesRead = 0;
                    if (!ReadProcessMemory(ProcessHandle, (LPCVOID)r.dwBaseAddr, buffer.get(), r.dwMemorySize, &bytesRead) || bytesRead == 0)
                        continue;

                    DWORD_PTR offset = 0;
                    while (offset + nSearchSize <= bytesRead)
                    {
                        int match = Memfind(buffer.get() + offset, (int)(bytesRead - offset), SearchBytes, (int)nSearchSize);
                        if (match == -1) break;
                        out.push_back(r.dwBaseAddr + offset + match);
                        offset += match + nSearchSize;
                    }
                }
            };

        // launch threads
        for (size_t i = 0; i < workerCount; ++i)
        {
            if (!workerRegions[i].empty())
                workers.emplace_back(workerProc, i);
        }

        for (auto& t : workers) if (t.joinable()) t.join();

        // merge results
        for (auto& v : workerResults)
            if (!v.empty())
                AddressRet.insert(AddressRet.end(), v.begin(), v.end());

        return !AddressRet.empty();
    }

    bool SlowFindPattern(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet)
    {

        BYTE* pCurrMemoryData = NULL;
        MEMORY_BASIC_INFORMATION	mbi;
        std::vector<MEMORY_REGION> m_vMemoryRegion;
        mbi.RegionSize = 0x1000;



        DWORD_PTR dwAddress = StartRange;
        DWORD_PTR nSearchSize = _msize(SearchBytes);


        while (VirtualQueryEx(ProcessHandle, (LPCVOID)dwAddress, &mbi, sizeof(mbi)) && (dwAddress < EndRange) && ((dwAddress + mbi.RegionSize) > dwAddress))
        {

            if ((mbi.State == MEM_COMMIT) && ((mbi.Protect & PAGE_GUARD) == 0) && (mbi.Protect != PAGE_NOACCESS) && ((mbi.AllocationProtect & PAGE_NOCACHE) != PAGE_NOCACHE))
            {

                MEMORY_REGION mData = { 0 };
                mData.dwBaseAddr = (DWORD_PTR)mbi.BaseAddress;
                mData.dwMemorySize = mbi.RegionSize;
                m_vMemoryRegion.push_back(mData);

            }
            dwAddress = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;

        }

        std::vector<MEMORY_REGION>::iterator it;
        for (it = m_vMemoryRegion.begin(); it != m_vMemoryRegion.end(); it++)
        {
            MEMORY_REGION mData = *it;


            DWORD_PTR dwNumberOfBytesRead = 0;
            pCurrMemoryData = new BYTE[mData.dwMemorySize];
            ZeroMemory(pCurrMemoryData, mData.dwMemorySize);
            ZwReadVirtualMemory(ProcessHandle, (LPVOID)mData.dwBaseAddr, pCurrMemoryData, mData.dwMemorySize, &dwNumberOfBytesRead);
            if ((int)dwNumberOfBytesRead <= 0)
            {
                delete[] pCurrMemoryData;
                continue;
            }
            DWORD_PTR dwOffset = 0;
            int iOffset = Memfind(pCurrMemoryData, dwNumberOfBytesRead, SearchBytes, nSearchSize);
            while (iOffset != -1)
            {
                dwOffset += iOffset;
                AddressRet.push_back(dwOffset + mData.dwBaseAddr);
                dwOffset += nSearchSize;
                iOffset = Memfind(pCurrMemoryData + dwOffset, dwNumberOfBytesRead - dwOffset - nSearchSize, SearchBytes, nSearchSize);
            }

            if (pCurrMemoryData != NULL)
            {
                delete[] pCurrMemoryData;
                pCurrMemoryData = NULL;
            }

        }
        return TRUE;
    }

    bool MidFindPattern(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, SIZE_T SearchSize, std::vector<DWORD_PTR>& AddressRet) {
        if (!SearchBytes || SearchSize == 0) return false;

        MEMORY_BASIC_INFORMATION mbi;
        DWORD_PTR dwAddress = StartRange;
        std::vector<MEMORY_REGION> memoryRegions;

        // collect memory regions
        while (dwAddress < EndRange && VirtualQueryEx(ProcessHandle, (LPCVOID)dwAddress, &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT && !(mbi.Protect & PAGE_GUARD) && mbi.Protect != PAGE_NOACCESS) {
                memoryRegions.push_back({ (DWORD_PTR)mbi.BaseAddress, (SIZE_T)mbi.RegionSize });
            }
            DWORD_PTR next = (DWORD_PTR)mbi.BaseAddress + (SIZE_T)mbi.RegionSize;
            if (next <= dwAddress) break; // safety
            dwAddress = next;
        }

        if (memoryRegions.empty()) return false;

        // cap threads so we don't spawn tons of threads for many small regions
        const size_t MAX_THREADS = 8; // tweakable
        size_t workerCount = std::min<size_t>(memoryRegions.size(), MAX_THREADS);

        // partition regions across workers (round-robin)
        std::vector<std::vector<MEMORY_REGION>> workerRegions(workerCount);
        for (size_t i = 0; i < memoryRegions.size(); ++i) {
            workerRegions[i % workerCount].push_back(memoryRegions[i]);
        }

        // per-worker result vectors (no locks)
        std::vector<std::vector<DWORD_PTR>> workerResults(workerCount);
        std::vector<std::thread> workers;
        workers.reserve(workerCount);

        // worker lambda does NOT touch AddressRet directly
        auto workerProc = [&](size_t workerIdx) {
            auto& regs = workerRegions[workerIdx];
            auto& out = workerResults[workerIdx];

            for (const auto& r : regs) {
                if (r.dwMemorySize == 0) continue;
                // allocate read buffer
                std::vector<BYTE> buf(r.dwMemorySize);
                SIZE_T bytesRead = 0;
                if (!ReadProcessMemory(ProcessHandle, (LPCVOID)r.dwBaseAddr, buf.data(), r.dwMemorySize, &bytesRead) || bytesRead == 0)
                    continue;

                SIZE_T offset = 0;
                while (offset + SearchSize <= bytesRead) {
                    int match = Memfind(buf.data() + offset, (int)(bytesRead - offset), SearchBytes, (int)SearchSize);
                    if (match == -1) break;
                    DWORD_PTR foundAddr = r.dwBaseAddr + offset + (DWORD_PTR)match;
                    out.push_back(foundAddr);
                    offset += (SIZE_T)match + SearchSize;
                }
            }
            };

        // spawn workers
        for (size_t i = 0; i < workerCount; ++i) {
            // if a worker has no regions, skip creating a thread (shouldn't usually happen)
            if (workerRegions[i].empty()) continue;
            workers.emplace_back(workerProc, i);
        }

        // join
        for (auto& t : workers) if (t.joinable()) t.join();

        // merge results into AddressRet
        for (auto& v : workerResults) {
            if (!v.empty()) AddressRet.insert(AddressRet.end(), v.begin(), v.end());
        }

        return !AddressRet.empty();
    }

    bool FastFindPattern(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet) {
        MEMORY_BASIC_INFORMATION mbi;
        DWORD_PTR dwAddress = StartRange;
        DWORD_PTR nSearchSize = _msize(SearchBytes);


        std::vector<MEMORY_REGION> memoryRegions;

        // Collect memory regions
        while (VirtualQueryEx(ProcessHandle, (LPCVOID)dwAddress, &mbi, sizeof(mbi)) && dwAddress < EndRange) {
            if ((mbi.State == MEM_COMMIT) && !(mbi.Protect & PAGE_GUARD) && mbi.Protect != PAGE_NOACCESS) {
                memoryRegions.push_back({ (DWORD_PTR)mbi.BaseAddress, mbi.RegionSize });
            }
            dwAddress = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
        }

        std::mutex mtx;
        auto processRegion = [&](MEMORY_REGION region) {
            std::unique_ptr<BYTE[]> memoryData(new BYTE[region.dwMemorySize]);
            SIZE_T bytesRead = 0;

            if (ReadProcessMemory(ProcessHandle, (LPCVOID)region.dwBaseAddr, memoryData.get(), region.dwMemorySize, &bytesRead) && bytesRead > 0) {
                DWORD_PTR offset = 0;
                int matchOffset;
                while ((matchOffset = Memfind(memoryData.get() + offset, bytesRead - offset, SearchBytes, nSearchSize)) != -1) {
                    std::lock_guard<std::mutex> lock(mtx);
                    AddressRet.push_back(region.dwBaseAddr + offset + matchOffset);
                    offset += matchOffset + nSearchSize;
                }
            }
            };

        // Launch threads for each memory region
        std::vector<std::future<void>> futures;
        for (const auto& region : memoryRegions) {
            futures.push_back(std::async(std::launch::async, processRegion, region));
        }

        // Wait for all threads to finish
        for (auto& fut : futures) {
            fut.get();
        }

        return true;
    }




    int Memfind(BYTE* buffer, DWORD_PTR dwBufferSize, BYTE* bstr, DWORD_PTR dwStrLen) {
        if (dwBufferSize < 0) {
            return -1;
        }
        DWORD_PTR  i, j;
        for (i = 0; i < dwBufferSize; i++) {
            for (j = 0; j < dwStrLen; j++) {
                if (buffer[i + j] != bstr[j] && bstr[j] != '?')
                    break;

            }
            if (j == dwStrLen)
                return i;
        }
        return -1;
    }

    struct EntityWallhackHere
    {
        DWORD_PTR addressWallhack;
        std::vector<BYTE> patternWallhack;
    };

    std::vector<EntityWallhackHere> OldWallhack;
    std::vector<DWORD_PTR> NewWallhack;

    std::unordered_map<uintptr_t, std::vector<BYTE>> WallhackoriginalBytesMap;

    struct EntitySpeedHere
    {
        DWORD_PTR addressSpeed;
        std::vector<BYTE> patternSpeed;
    };

    std::vector<EntitySpeedHere> OldSpeed;
    std::vector<DWORD_PTR> NewSpeed;

    std::unordered_map<uintptr_t, std::vector<BYTE>> SpeedoriginalBytesMap;

    bool FindPatternByMode(DWORD_PTR startAddress, DWORD_PTR endAddress, BYTE* pattern, SIZE_T patternSize, std::vector<DWORD_PTR>& out)
    {
        if (!pattern || patternSize == 0) return false;
        if (fastinject) {
            return MidFindPattern(startAddress, endAddress, pattern, patternSize, out);
        }

        // Slow scanner expects heap array size via _msize; copy bytes to dedicated buffer.
        BYTE* slowPattern = new BYTE[patternSize];
        memcpy(slowPattern, pattern, patternSize);
        bool ok = SlowFindPattern(startAddress, endAddress, slowPattern, out);
        delete[] slowPattern;
        return ok && !out.empty();
    }

    bool SaveSpeedAoB()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR startAddress = reinterpret_cast<DWORD_PTR>(si.lpMinimumApplicationAddress);
        DWORD_PTR endAddress = reinterpret_cast<DWORD_PTR>(si.lpMaximumApplicationAddress);

        std::vector<BYTE> SearchSpeed = { 0x02, 0x2B, 0x07, 0x3D, 0x02, 0x2B, 0x07, 0x3D, 0x02, 0x2B, 0x07, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x9B, 0x6C, 0xF2, 0x41 };

        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return false;
        }

        MemoryLogs = "Speed Hack Scan: Scanning!";

        NewSpeed.clear();
        OldSpeed.clear();

        if (!FindPatternByMode(startAddress, endAddress, SearchSpeed.data(), SearchSpeed.size(), NewSpeed))
        {
            MemoryLogs = "Pattern search failed.";
            return false;
        }

        if (NewSpeed.empty())
        {
            MemoryLogs = "Speed Pattern Not Found!";
            return false;
        }

        MemoryLogs = "Speed Hack : Loaded Successfully! Scan Results Found";
        return true;
    }

    bool ActivateSpeed()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = { 0xE3, 0xA5, 0x9B, 0x3C, 0xE3, 0xA5, 0x9B, 0x3C, 0x02, 0x2B, 0x07, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x9B, 0x6C, 0xF2, 0x41 };

        if (NewSpeed.empty()) {
            if (!SaveSpeedAoB())
                return false;
        }

        MemoryLogs = "Speed Hack: Scanning...";

        for (const auto& address : NewSpeed) {
            if (!WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replace.data(), replace.size(), nullptr)) {
                MemoryLogs = "Speed Hack: Failed to write memory at address";
                continue;
            }
            MemoryLogs = "Speed Hack : Activated Successfully!";
        }

        return true;
    }

    bool OFFSpeed()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = { 0x02, 0x2B, 0x07, 0x3D, 0x02, 0x2B, 0x07, 0x3D, 0x02, 0x2B, 0x07, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x9B, 0x6C, 0xF2, 0x41 };

        if (NewSpeed.empty()) {
            MemoryLogs = "Speed Load: Address Not Found!";
            return false;
        }

        MemoryLogs = "Speed Hack: Reverting...";

        for (const auto& address : NewSpeed) {
            if (!WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replace.data(), replace.size(), nullptr)) {
                MemoryLogs = "Speed Hack: Failed to write memory at address";
                continue;
            }
            MemoryLogs = "Speed Hack : Deactivated Successfully!";
        }

        return true;
    }


    struct EntityCameraHere
    {
        DWORD_PTR addressCamera;
        std::vector<BYTE> patternCamera;
    };

    std::vector<EntityCameraHere> OldCamera;
    std::vector<DWORD_PTR> NewCamera;

    std::unordered_map<uintptr_t, std::vector<BYTE>> CameraOriginalBytesMap;

    bool SaveCameraAoB()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR startAddress = reinterpret_cast<DWORD_PTR>(si.lpMinimumApplicationAddress);
        DWORD_PTR endAddress = reinterpret_cast<DWORD_PTR>(si.lpMaximumApplicationAddress);

        std::vector<BYTE> SearchCamera = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00
        };

        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return false;
        }

        MemoryLogs = "Camera Scan: Scanning!";

        NewCamera.clear();
        OldCamera.clear();

        if (!FindPatternByMode(startAddress, endAddress, SearchCamera.data(), SearchCamera.size(), NewCamera))
        {
            MemoryLogs = "Camera pattern search failed.";
            return false;
        }

        if (NewCamera.empty())
        {
            MemoryLogs = "Camera Pattern Not Found!";
            return false;
        }

        MemoryLogs = "Camera : Loaded Successfully!";
        return true;
    }

    bool ActivateCamera()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00
        };

        if (NewCamera.empty()) {
            MemoryLogs = "Camera Load: Address Not Found!";
            return false;
        }

        MemoryLogs = "Camera: Activating...";

        for (const auto& address : NewCamera) {
            if (!WriteProcessMemory(ProcessHandle,
                reinterpret_cast<LPVOID>(address),
                replace.data(),
                replace.size(),
                nullptr))
            {
                MemoryLogs = "Camera: Failed to write memory";
                continue;
            }

            MemoryLogs = "Camera : Activated Successfully!";
        }

        return true;
    }

    bool OFFCamera()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = {
           0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00
        };

        if (NewCamera.empty()) {
            MemoryLogs = "Camera Load: Address Not Found!";
            return false;
        }

        MemoryLogs = "Camera: Reverting...";

        for (const auto& address : NewCamera) {
            if (!WriteProcessMemory(ProcessHandle,
                reinterpret_cast<LPVOID>(address),
                replace.data(),
                replace.size(),
                nullptr))
            {
                MemoryLogs = "Camera: Failed to write memory";
                continue;
            }

            MemoryLogs = "Camera : Deactivated Successfully!";
        }

        return true;
    }


    struct EntityFastlandingHere
    {
        DWORD_PTR addressFastlanding;
        std::vector<BYTE> patternFastlanding;
    };

    std::vector<EntityFastlandingHere> OldFastlanding;
    std::vector<DWORD_PTR> NewFastlanding;

    std::unordered_map<uintptr_t, std::vector<BYTE>> FastlandingoriginalBytesMap;

    bool SaveFastlandingAoB()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR startAddress = reinterpret_cast<DWORD_PTR>(si.lpMinimumApplicationAddress);
        DWORD_PTR endAddress = reinterpret_cast<DWORD_PTR>(si.lpMaximumApplicationAddress);

        std::vector<BYTE> SearchFastlanding = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0xFF };

        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return false;
        }

        MemoryLogs = "Pattern : Scanning!";

        NewFastlanding.clear();
        OldFastlanding.clear();

        if (!FindPatternByMode(startAddress, endAddress, SearchFastlanding.data(), SearchFastlanding.size(), NewFastlanding))
        {
            MemoryLogs = "Pattern search failed.";
            return false;
        }

        if (NewFastlanding.empty())
        {
            MemoryLogs = "Pattern search failed.";
            return false;
        }

        MemoryLogs = "Scan Results Found";
        return true;
    }

    bool ActivateFastlanding()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0xFF };

        if (NewFastlanding.empty()) {
            MemoryLogs = "Fast Land: load pattern first!";
            return false;
        }

        MemoryLogs = "Scan results found. Replacing values...";

        for (const auto& address : NewFastlanding)
        {
            if (!WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replace.data(), replace.size(), nullptr))
            {
                MemoryLogs = "Pattern search failed.";
                continue;
            }
            MemoryLogs = "Successfully applied";
        }

        return true;
    }

    bool OFFFastlanding()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0x7F, 0x00, 0x00, 0x80, 0xFF };

        if (NewFastlanding.empty()) {
            MemoryLogs = "Fast Land: nothing to restore!";
            return false;
        }

        MemoryLogs = "Scan results found. Replacing values...";

        for (const auto& address : NewFastlanding)
        {
            if (!WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replace.data(), replace.size(), nullptr))
            {
                MemoryLogs = "Pattern search failed.";
                continue;
            }

            MemoryLogs = "Successfully Removed..";
        }

        return true;
    }

    bool SaveWallhackAoB()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR startAddress = reinterpret_cast<DWORD_PTR>(si.lpMinimumApplicationAddress);
        DWORD_PTR endAddress = reinterpret_cast<DWORD_PTR>(si.lpMaximumApplicationAddress);

        std::vector<BYTE> SearchWallhack = { 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xEF, 0xFF, 0x7F, 0x3F, 0xAE, 0x47, 0x81, 0x3F };

        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found!";
            return false;
        }

        MemoryLogs = "Wall Hack Scan: Scanning!";

        NewWallhack.clear();
        OldWallhack.clear();

        if (!FindPatternByMode(startAddress, endAddress, SearchWallhack.data(), SearchWallhack.size(), NewWallhack))
        {
            MemoryLogs = "Pattern search failed.";
            return false;
        }

        if (NewWallhack.empty())
        {
            MemoryLogs = "Pattern search failed.";
            return false;
        }

        MemoryLogs = "Wall Hack : Loaded Successfully! Scan Results Found";
        return true;
    }

    bool ActivateWallhack()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = { 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xAE, 0x47, 0x81, 0xBF, 0xAE, 0x47, 0x81, 0x3F, 0xEF, 0xFF, 0x7F, 0x3F, 0xAE, 0x47, 0x81, 0x3F };

        if (NewWallhack.empty()) {
            if (!SaveWallhackAoB())
                return false;
        }

        MemoryLogs = "Scan results found. Replacing values...";

        for (const auto& address : NewWallhack)
        {
            if (!WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replace.data(), replace.size(), nullptr))
            {
                MemoryLogs = "Pattern search failed.";
                continue;
            }
            MemoryLogs = "Wall Hack : Activated Successfully!";
        }

        return true;
    }

    bool OFFWallhack()
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        std::vector<BYTE> replace = { 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xAE, 0x47, 0x81, 0x3F, 0xEF, 0xFF, 0x7F, 0x3F, 0xAE, 0x47, 0x81, 0x3F };

        if (NewWallhack.empty()) {
            MemoryLogs = "Wall Hack: load pattern first!";
            return false;
        }

        MemoryLogs = "Scan results found. Replacing values...";

        for (const auto& address : NewWallhack)
        {
            if (!WriteProcessMemory(ProcessHandle, reinterpret_cast<LPVOID>(address), replace.data(), replace.size(), nullptr))
            {
                MemoryLogs = "Pattern search failed.";
                continue;
            }

            MemoryLogs = "Wall Hack : Deactivated Successfully!";
        }

        return true;
    }

};


