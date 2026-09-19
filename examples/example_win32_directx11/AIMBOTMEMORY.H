#pragma once
#pragma once
#include <Windows.h>
#include <vector>
#include <string> 
#include <iostream>
#include <TlHelp32.h>
#include <tchar.h>
#include <emmintrin.h>  // SSE2
#include <immintrin.h>  // AVX
#include <atlbase.h>
#include <random>
#include <thread>
#include <mutex>
#include <future>
#include <immintrin.h> // For SIMD
#include <tchar.h>     // still needed for PROCESSENTRY32



extern std::string MemoryLogs;



class AimMemory
{

public:



    const char* GetEmulatorRunning() {
        if (GetPid("HD-Player.exe") != 0)
            return "HD-Player.exe";

        else if (GetPid("MEmuHeadless.exe") != 0)
            return "MEmuHeadless.exe";

        else if (GetPid("LdVBoxHeadless.exe") != 0)
            return "LdVBoxHeadless.exe";

        else if (GetPid("AndroidProcess.exe") != 0)
            return "AndroidProcess.exe";

        else if (GetPid("Nox.exe") != 0)
            return "Nox.exe";
    }


    DWORD ProcessId = 0;
    HANDLE ProcessHandle;
    std::mutex mtx;

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

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe;  // Changed to W version
        pe.dwSize = sizeof(PROCESSENTRY32W);  // Changed to W version

        Process32FirstW(hSnap, &pe);  // Changed to W version
        do {
            if (_wcsicmp(pe.szExeFile, wProcName) == 0) {
                if ((int)pe.cntThreads > threadCount) {
                    threadCount = pe.cntThreads;
                    pid = pe.th32ProcessID;
                }
            }
        } while (Process32NextW(hSnap, &pe));  // Changed to W version

        return pid;
    }


    static int findMyProc(const char* procname) {
        if (procname == NULL)
            return 0;

        wchar_t wProcName[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, procname, -1, wProcName, MAX_PATH);

        DWORD pid = 0;
        DWORD threadCount = 0;

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
            return 0;

        PROCESSENTRY32W pe;  // Use the Wide version
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnap, &pe)) {  // Use Wide version of the function
            do {
                if (_wcsicmp(pe.szExeFile, wProcName) == 0) {
                    if (pe.cntThreads > threadCount) {
                        threadCount = pe.cntThreads;
                        pid = pe.th32ProcessID;
                    }
                }
            } while (Process32NextW(hSnap, &pe));  // Use Wide version
        }

        CloseHandle(hSnap);
        return pid;
    }

    BOOL AttackProcess(const char* procname) {
        DWORD ProcId = GetPid(procname);
        if (ProcId == 0)
            return false;

        ProcessId = ProcId;
        ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, 0, ProcessId);
        return ProcessHandle != nullptr;
    }






    std::unordered_map<DWORD_PTR, int> originalValuesWrite;
    std::unordered_map<DWORD_PTR, int> originalValuesWrite2;
    std::unordered_map<DWORD_PTR, int> modifiedValuesWrite;
    std::unordered_map<DWORD_PTR, int> modifiedValuesWrite2;
    std::unordered_map<DWORD_PTR, int> modifiedAoBs;




    std::vector<DWORD_PTR> AddressScan;

    std::vector<BYTE> ScanAimbot = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', 0x00, 0x00, 0x00, 0x00, 0xA5, 0x43 };


    void EnableAim(const std::string& mode)
    {



        originalValuesWrite.clear();
        originalValuesWrite2.clear();
        modifiedValuesWrite.clear();
        modifiedValuesWrite2.clear();
        AddressScan.clear();


        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD_PTR SAddress = (DWORD_PTR)si.lpMinimumApplicationAddress;
        DWORD_PTR EAddress = (DWORD_PTR)si.lpMaximumApplicationAddress;

        // --- Check if emulator/process is running ---
        if (!AttackProcess(GetEmulatorRunning()))
        {
            MemoryLogs = "Emulator Not Found";
            return;
        }

        MemoryLogs = "Aimbot " + mode + " : Applying!";

        // --- Pattern scan to find addresses ---
        if (fastinject)
        {
            FastFindPattern(SAddress, EAddress, ScanAimbot.data(), AddressScan);
        }
        else
        {
            SlowFindPattern(SAddress, EAddress, ScanAimbot.data(), AddressScan);
        }

        if (AddressScan.empty())
        {
            MemoryLogs = "Address Not Found!";
            return;
        }

        // --- Loop over all found addresses ---
        for (size_t i = 0; i < AddressScan.size(); i++)
        {
            DWORD_PTR addressscan, addressrep;

            if (mode == "LeftNeck")
            {
                addressscan = AddressScan[i] + 0x188; // Neck Left
                addressrep = AddressScan[i] + 0x154;  // Chest
            }
            else if (mode == "RightNeck")
            {
                addressscan = AddressScan[i] + 0x184; // Neck Right
                addressrep = AddressScan[i] + 0x154;  // Chest
            }
            else if (mode == "LeftShoulder")
            {
                addressscan = AddressScan[i] + 0x194; // Left Shoulder
                addressrep = AddressScan[i] + 0x154;  // Chest
            }
            else if (mode == "RightShoulder")
            {
                addressscan = AddressScan[i] + 0x198; // Right Shoulder
                addressrep = AddressScan[i] + 0x154;  // Chest
            }
            else if (mode == "Neck")
            {
                addressscan = AddressScan[i] + 0xB6; // Neck (center)
                addressrep = AddressScan[i] + 0xB2;  // Chest
            }
            else // Default fallback
            {
                addressscan = AddressScan[i] + 0xB6; // Neck default
                addressrep = AddressScan[i] + 0xB2;  // Chest
            }

            int bufferRead, bufferWrite;

            // --- Read current values to backup ---
            if (!SafeReadProcessMemory(addressrep, bufferWrite))
            {
                MemoryLogs = "Failed to read memory at addressrep";
                return;
            }
            originalValuesWrite[addressrep] = bufferWrite;

            if (!SafeReadProcessMemory(addressscan, bufferRead))
            {
                MemoryLogs = "Failed to read memory at addressscan";
                return;
            }
            originalValuesWrite2[addressscan] = bufferRead;

            // --- Swap values to enable aim assist hack ---
            if (!SafeWriteProcessMemory(addressrep, bufferRead))
            {
                MemoryLogs = "Failed to write memory at addressrep";
                return;
            }
            modifiedValuesWrite[addressrep] = bufferRead;

            if (!SafeWriteProcessMemory(addressscan, bufferWrite))
            {
                MemoryLogs = "Failed to write memory at addressscan";
                return;
            }
            modifiedValuesWrite2[addressscan] = bufferWrite;
        }

        MemoryLogs = "Aimbot " + mode + " : Applied Successfully!";
    }



    bool SafeReadProcessMemory(DWORD_PTR address, int& buffer) {
        if (ReadProcessMemory(ProcessHandle, (LPVOID)address, &buffer, sizeof(buffer), NULL)) {
            return true;
        }
        return false;
    }

    // Safe memory write function with error handling
    bool SafeWriteProcessMemory(DWORD_PTR address, int& buffer) {
        if (WriteProcessMemory(ProcessHandle, (LPVOID)address, &buffer, sizeof(buffer), 0)) {
            return true;
        }
        return false;
    }



    void Restore() {
        if (!AttackProcess(GetEmulatorRunning())) {
            return;
        }
        for (const auto& entry : originalValuesWrite) {
            DWORD_PTR address = entry.first;
            int originalValue = entry.second;
            WriteProcessMemory(ProcessHandle, (LPVOID)address, &originalValue, sizeof(originalValue), NULL);
        }
        for (const auto& entry : originalValuesWrite2) {
            DWORD_PTR address = entry.first;
            int originalValue = entry.second;
            WriteProcessMemory(ProcessHandle, (LPVOID)address, &originalValue, sizeof(originalValue), NULL);
        }
        CloseHandle(ProcessHandle);
    }

    void Reapply() {
        if (!AttackProcess(GetEmulatorRunning())) {
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
        CloseHandle(ProcessHandle);
    }

    bool ChangeProtection(ULONG Address, size_t size, DWORD NewProtect, DWORD& OldProtect)
    {
        return VirtualProtectEx(ProcessHandle, (LPVOID)Address, size, NewProtect, &OldProtect);;
    }


    bool ReplacePattern(DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* SearchAob, BYTE* ReplaceAob)
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {

            return false;
        }

        AttackProcess(GetEmulatorRunning());

        int RepByteSize = _msize(ReplaceAob);
        if (RepByteSize <= 0) return false;
        std::vector<DWORD_PTR> foundedAddress;
        FindPattern(dwStartRange, dwEndRange, SearchAob, foundedAddress);
        if (foundedAddress.empty())
            return false;

        //OutputDebugStringA(std::to_string(foundedAddress.size()).c_str());

        DWORD OldProtect;
        for (int i = 0; i < foundedAddress.size(); i++)
        {
            ChangeProtection(foundedAddress[i], RepByteSize, PAGE_EXECUTE_READWRITE, OldProtect);
            WriteProcessMemory(ProcessHandle, (LPVOID)foundedAddress[i], ReplaceAob, RepByteSize, 0);
        }

        return true;
    }


    bool ReplaceCustom(DWORD_PTR dwStartRange, DWORD_PTR dwEndRange, BYTE* SearchAob, BYTE* ReplaceAob, bool ForceWrite = false)
    {
        int RepByteSize = _msize(ReplaceAob);
        if (RepByteSize <= 0) return false;
        std::vector<DWORD_PTR> foundedAddress;
        FindPattern(dwStartRange, dwEndRange, SearchAob, foundedAddress);
        if (foundedAddress.empty())
            return false;

        if (foundedAddress.size() > 1)
        {
            return false;
        }
        else
        {
            DWORD OldProtect;
            for (int i = 0; i < foundedAddress.size(); i++)
            {
                ChangeProtection(foundedAddress[i], RepByteSize, PAGE_EXECUTE_READWRITE, OldProtect);
                WriteProcessMemory(ProcessHandle, (LPVOID)foundedAddress[i], ReplaceAob, RepByteSize, 0);
            }

            return true;
        }
    }

    bool FindPattern(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet)
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {

            return false;
        }

        AttackProcess(GetEmulatorRunning());

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
            ReadProcessMemory(ProcessHandle, (LPCVOID)mData.dwBaseAddr, pCurrMemoryData, mData.dwMemorySize, &dwNumberOfBytesRead);
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

    bool ThreadedFindPattern(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet)
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        AttackProcess(GetEmulatorRunning());

        MEMORY_BASIC_INFORMATION mbi;
        std::vector<MEMORY_REGION> m_vMemoryRegion;
        DWORD_PTR dwAddress = StartRange;
        size_t nSearchSize = _msize(SearchBytes);

        while (VirtualQueryEx(ProcessHandle, (LPCVOID)dwAddress, &mbi, sizeof(mbi)) &&
            dwAddress < EndRange &&
            ((dwAddress + mbi.RegionSize) > dwAddress))
        {
            if ((mbi.State == MEM_COMMIT) &&
                !(mbi.Protect & PAGE_GUARD) &&
                mbi.Protect != PAGE_NOACCESS &&
                !(mbi.AllocationProtect & PAGE_NOCACHE))
            {
                MEMORY_REGION mData = { 0 };
                mData.dwBaseAddr = (DWORD_PTR)mbi.BaseAddress;
                mData.dwMemorySize = mbi.RegionSize;
                m_vMemoryRegion.push_back(mData);
            }
            dwAddress = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
        }

        std::atomic<int> regionIndex = 0;
        std::mutex resultLock;
        const int maxThreads = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        for (int t = 0; t < maxThreads; ++t)
        {
            threads.emplace_back([&]() {
                int idx;
                while ((idx = regionIndex++) < m_vMemoryRegion.size())
                {
                    const auto& mData = m_vMemoryRegion[idx];

                    std::vector<BYTE> buffer(mData.dwMemorySize);
                 //   if (!Mem.ReadArray(mData.dwBaseAddr, buffer))
                        continue;

                    DWORD_PTR dwOffset = 0;
                    int iOffset = Memfind(buffer.data(), mData.dwMemorySize, SearchBytes, nSearchSize);

                    while (iOffset != -1)
                    {
                        dwOffset += iOffset;
                        {
                            std::lock_guard<std::mutex> lock(resultLock);
                            AddressRet.push_back(dwOffset + mData.dwBaseAddr);
                        }
                        dwOffset += nSearchSize;
                        iOffset = Memfind(buffer.data() + dwOffset, mData.dwMemorySize - dwOffset - nSearchSize, SearchBytes, nSearchSize);
                    }
                }
                });
        }

        for (auto& t : threads)
            t.join();

        return !AddressRet.empty();
    }


    bool SlowFindPattern(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet)
    {
        if (!AttackProcess(GetEmulatorRunning()))
        {

            return false;
        }

        AttackProcess(GetEmulatorRunning());

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
            ReadProcessMemory(ProcessHandle, (LPCVOID)mData.dwBaseAddr, pCurrMemoryData, mData.dwMemorySize, &dwNumberOfBytesRead);
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

    bool SlowFindPattern2(DWORD_PTR StartRange, DWORD_PTR EndRange, BYTE* SearchBytes, std::vector<DWORD_PTR>& AddressRet)
    {
        if (!AttackProcess(GetEmulatorRunning()))
            return false;

        AttackProcess(GetEmulatorRunning());

        //static std::unordered_map<DWORD_PTR, int> modifiedAoBs; // Internally keeps track of found addresses

        BYTE* pCurrMemoryData = nullptr;
        MEMORY_BASIC_INFORMATION mbi;
        std::vector<MEMORY_REGION> m_vMemoryRegion;
        mbi.RegionSize = 0x1000;
        DWORD_PTR dwAddress = StartRange;
        DWORD_PTR nSearchSize = _msize(SearchBytes);

        // Collect memory regions
        while (VirtualQueryEx(ProcessHandle, (LPCVOID)dwAddress, &mbi, sizeof(mbi)) &&
            (dwAddress < EndRange) && ((dwAddress + mbi.RegionSize) > dwAddress))
        {
            if ((mbi.State == MEM_COMMIT) &&
                ((mbi.Protect & PAGE_GUARD) == 0) &&
                (mbi.Protect != PAGE_NOACCESS) &&
                ((mbi.AllocationProtect & PAGE_NOCACHE) != PAGE_NOCACHE))
            {
                MEMORY_REGION mData = { (DWORD_PTR)mbi.BaseAddress, mbi.RegionSize };
                m_vMemoryRegion.push_back(mData);
            }

            dwAddress = (DWORD_PTR)mbi.BaseAddress + mbi.RegionSize;
        }

        // Search in each region
        for (const auto& mData : m_vMemoryRegion)
        {
            DWORD_PTR dwNumberOfBytesRead = 0;
            pCurrMemoryData = new BYTE[mData.dwMemorySize];
            ZeroMemory(pCurrMemoryData, mData.dwMemorySize);

            ReadProcessMemory(ProcessHandle, (LPCVOID)mData.dwBaseAddr, pCurrMemoryData, mData.dwMemorySize, &dwNumberOfBytesRead);

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
                DWORD_PTR firstByteAddress = dwOffset + mData.dwBaseAddr;

                // Only add if not already found
                if (modifiedAoBs.find(firstByteAddress) == modifiedAoBs.end())
                {
                    AddressRet.push_back(firstByteAddress);
                    modifiedAoBs[firstByteAddress] = 1;
                }

                dwOffset += nSearchSize;
                iOffset = Memfind(pCurrMemoryData + dwOffset, dwNumberOfBytesRead - dwOffset - nSearchSize, SearchBytes, nSearchSize);
            }

            delete[] pCurrMemoryData;
            pCurrMemoryData = nullptr;
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


};
