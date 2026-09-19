#include "ProcessUtils.hpp"
#include <iostream>

namespace ProcessUtils {

    DWORD GetProc(const wchar_t* processName)
    {
        DWORD processID = 0;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnapshot, &pe32))
            {
                do
                {
                    if (wcscmp(pe32.szExeFile, processName) == 0)
                    {
                        processID = pe32.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(hSnapshot, &pe32));
            }
            CloseHandle(hSnapshot);
        }
        return processID;
    }

    bool CheckProcessInstances(const wchar_t* processName, int& count) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot, &pe32))
        {
            do {
                if (wcscmp(pe32.szExeFile, processName) == 0) {
                    count++;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }

        CloseHandle(hSnapshot);
        return true;
    }

    bool KillProc(DWORD processID)
    {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
        if (hProcess == NULL)
        {
            return false;
        }
        bool result = TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
        return result;
    }

    bool IsProcRun(const wchar_t* processName, DWORD& processID)
    {
        bool isRunning = false;
        processID = 0;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return false;
        }
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot, &pe32))
        {
            do {
                if (wcscmp(pe32.szExeFile, processName) == 0) {
                    isRunning = true;
                    processID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);

        return isRunning;
    }

    bool ForgeKillProc(DWORD processID)
    {
        HWND hwnd = NULL;
        DWORD dwPID = 0;
        do {
            hwnd = FindWindowEx(NULL, hwnd, NULL, NULL);
            GetWindowThreadProcessId(hwnd, &dwPID);
        } while (dwPID != processID && hwnd != NULL);

        if (hwnd != NULL)
        {
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            return true;
        }
        return false;
    }

}