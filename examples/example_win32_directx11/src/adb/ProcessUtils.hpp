#ifndef PROCESS_UTILS_HPP
#define PROCESS_UTILS_HPP

#include <Windows.h>
#include <tlhelp32.h>

namespace ProcessUtils {

    DWORD GetProc(const wchar_t* processName);

    bool CheckProcessInstances(const wchar_t* processName, int& count);

    bool KillProc(DWORD processID);

    bool IsProcRun(const wchar_t* processName, DWORD& processID);

    bool ForgeKillProc(DWORD processID);
}

#endif
