#pragma once

#include <Windows.h>
#include <unordered_map>
#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <winnt.h>

class MemoryUtils {
public:
    inline static void* vmPtr = nullptr;
    inline static void* pVMAddr = nullptr;
    inline static void* cpuAddr = nullptr;

    inline static std::unordered_map<uintptr_t, uintptr_t> Cache;

    using PGMPhysReadFunc = int(__cdecl*)(void*, uintptr_t, void*, size_t);
    using VMMGetCpuByIdFunc = void* (__cdecl*)(void*, int);
    using PGMPhysGCPtr2GCPhysFunc = int(__cdecl*)(void*, uintptr_t, uintptr_t*);
    using PGMPhysSimpleWriteGCPhysFunc = int(__cdecl*)(void*, uintptr_t, void*, size_t);

    inline static PGMPhysReadFunc ogPhysRead = nullptr;
    inline static VMMGetCpuByIdFunc ogCPU = nullptr;
    inline static PGMPhysGCPtr2GCPhysFunc ogCast = nullptr;
    inline static PGMPhysSimpleWriteGCPhysFunc ogWrite = nullptr;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
        buffer->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static int __cdecl HookedPGMPhysRead(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t cbRead) {
        if (!vmPtr) {
            vmPtr = pVM;
#ifdef _DEBUG
            std::cout << "Memory initialized: " << vmPtr << std::endl;
#endif
        }
        if (!ogPhysRead)
            return -1;
        return ogPhysRead(pVM, GCPhys, pvBuf, cbRead);
    }

    static int HookWrite(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t cbRead) {
        if (!ogWrite)
            return -1;
        return ogWrite(pVM, GCPhys, pvBuf, cbRead);
    }

    static int HookRead(void* pVM, uintptr_t GCPhys, void* pvBuf, size_t cbRead) {
        if (!ogPhysRead)
            return -1;
        return ogPhysRead(pVM, GCPhys, pvBuf, cbRead);
    }

    static void* CPU(void* pVM, int cpuId) {
        if (!ogCPU)
            return nullptr;
        return ogCPU(pVM, cpuId);
    }

    static int Cast(void* pVCpu, uintptr_t address, uintptr_t* physAddress) {
        if (!ogCast)
            return -1;
        return ogCast(pVCpu, address, physAddress);
    }

    static bool IsReady() {
        return pVMAddr != nullptr && ogPhysRead != nullptr && ogCPU != nullptr && ogCast != nullptr;
    }

    static void Initialize(void* pVM) {
        pVMAddr = pVM;
        cpuAddr = CPU(pVM, 0);
        Cache.clear();
    }

    static constexpr uint32_t MAX_CPU = 4U;
    static constexpr uintptr_t MIN_VALID_ADDRESS = 0x1000;

    static bool Convert(uintptr_t address, uintptr_t& phys) {
        phys = 0;
        if (!IsReady() || address <= MIN_VALID_ADDRESS)
            return false;

        auto it = Cache.find(address);
        if (it != Cache.end() && it->second != 0) {
            phys = it->second;
            return true;
        }

        for (uint32_t i = 0; i < MAX_CPU; ++i) {
            void* cpu = CPU(pVMAddr, i);
            if (!cpu) continue;

            uintptr_t tempPhys = 0;
            if (Cast(cpu, address, &tempPhys) == 0) {
                phys = tempPhys;
                Cache[address] = tempPhys;
                return true;
            }
        }
        return false;
    }

    template<typename T>
    static T ReadS(uintptr_t address) {
        T result{};
        if (!IsReady())
            return result;

        uintptr_t physAddress;
        if (Convert(address, physAddress))
            HookRead(pVMAddr, physAddress, &result, sizeof(T));
        return result;
    }

    template<typename T>
    static bool Read(uintptr_t address, T& data) {
        if (!IsReady())
            return false;
        uintptr_t physAddress;
        if (!Convert(address, physAddress))
            return false;
        return HookRead(pVMAddr, physAddress, &data, sizeof(T)) == 0;
    }

    template<typename T>
    static void Write(uintptr_t address, const T& value) {
        if (!IsReady())
            return;
        uintptr_t physAddress;
        if (Convert(address, physAddress))
            HookWrite(pVMAddr, physAddress, (void*)&value, sizeof(T));
    }

    static bool WriteRaw(uintptr_t address, const void* data, size_t size) {
        if (!IsReady())
            return false;
        uintptr_t physAddress;
        if (!Convert(address, physAddress))
            return false;
        return HookWrite(pVMAddr, physAddress, (void*)data, size) == 0;
    }

    template<typename T>
    static bool ReadArray(uintptr_t address, std::vector<T>& array) {
        uintptr_t physAddress;
        if (!Convert(address, physAddress)) return false;
        return HookRead(pVMAddr, physAddress, array.data(), sizeof(T) * array.size()) == 0;
    }

    static std::string Utf16ToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), out.data(), len, nullptr, nullptr);
        return out;
    }

    static std::string ReadString(uintptr_t address, int size, bool unicode = false) {
        if (size <= 0) return "";
        std::vector<uint8_t> stringBytes(size);
        if (!ReadArray(address, stringBytes)) return "";

        if (unicode) {
            const int wcharCount = size / static_cast<int>(sizeof(wchar_t));
            if (wcharCount <= 0) return "";
            std::wstring wide(reinterpret_cast<const wchar_t*>(stringBytes.data()), wcharCount);
            const size_t nullPos = wide.find(L'\0');
            if (nullPos != std::wstring::npos)
                wide.resize(nullPos);
            return Utf16ToUtf8(wide);
        }

        std::string readString(reinterpret_cast<char*>(stringBytes.data()), stringBytes.size());
        const size_t nullPos = readString.find('\0');
        if (nullPos != std::string::npos)
            readString.resize(nullPos);
        return readString;
    }

    template<typename T>
    static bool ReadArray2(uintptr_t address, std::vector<T>& array) {
        uintptr_t convertedAddress;
        if (!Convert(address, convertedAddress)) return false;
        return HookRead(pVMAddr, convertedAddress, array.data(), sizeof(T) * array.size()) == 0;
    }

    static std::string ReadString2(uintptr_t address, int size, bool unicode = true) {
        if (size <= 0) return "";
        std::vector<uint8_t> stringBytes(size);
        if (!ReadArray2(address, stringBytes)) return "";

        if (unicode) {
            const int wcharCount = size / static_cast<int>(sizeof(wchar_t));
            if (wcharCount <= 0) return "";
            std::wstring wide(reinterpret_cast<const wchar_t*>(stringBytes.data()), wcharCount);
            const size_t nullPos = wide.find(L'\0');
            if (nullPos != std::wstring::npos)
                wide.resize(nullPos);
            return Utf16ToUtf8(wide);
        }

        std::string readString(reinterpret_cast<char*>(stringBytes.data()), stringBytes.size());
        const size_t nullPos = readString.find('\0');
        if (nullPos != std::string::npos)
            readString.resize(nullPos);
        return readString;
    }

    // Unity IL2CPP player name object (same layout as Project Atom).
    static bool Utf8HasReplacementChar(const std::string& s) {
        return s.find('\xEF') != std::string::npos && s.find("\xEF\xBF\xBD") != std::string::npos;
    }

    static std::string TrimNull(std::string s) {
        const size_t n = s.find('\0');
        if (n != std::string::npos)
            s.resize(n);
        while (!s.empty() && (static_cast<unsigned char>(s.back()) < 32))
            s.pop_back();
        return s;
    }

    static std::string ReadUnityPlayerName(uintptr_t nameObject) {
        if (!nameObject) return {};
        int nameLength = 0;
        if (!Read(nameObject + 0x8, nameLength) || nameLength <= 0 || nameLength > 64)
            return {};

        auto tryRead = [&](int byteSize) -> std::string {
            if (byteSize <= 0 || byteSize > 512) return {};
            return TrimNull(ReadString2(nameObject + 0xC, byteSize, true));
        };

        std::string name = tryRead(nameLength * 5);
        if (name.empty() || Utf8HasReplacementChar(name))
            name = tryRead((nameLength + 1) * static_cast<int>(sizeof(wchar_t)));
        if (name.empty() || Utf8HasReplacementChar(name))
            name = tryRead(nameLength * static_cast<int>(sizeof(wchar_t)));
        return name;
    }

    static std::string ReadUnityPlayerNameFromEntity(uintptr_t entity, uintptr_t nameOffset) {
        uint32_t nameAddr = 0;
        if (!Read(entity + nameOffset, nameAddr) || !nameAddr)
            return {};
        return ReadUnityPlayerName(nameAddr);
    }

    // Find pattern in memory using AOB (Array of Bytes)
    static uintptr_t FindPattern(uintptr_t moduleBase, const uint8_t* pattern, size_t patternSize) {
        if (!moduleBase || !pattern || patternSize == 0) return 0;
        
        // Read module header to get size
        IMAGE_DOS_HEADER dosHeader;
        if (!Read(moduleBase, dosHeader)) return 0;
        
        IMAGE_NT_HEADERS ntHeaders;
        if (!Read(moduleBase + dosHeader.e_lfanew, ntHeaders)) return 0;
        
        uintptr_t moduleSize = ntHeaders.OptionalHeader.SizeOfImage;
        if (moduleSize == 0) return 0;
        
        // Search through module memory
        for (uintptr_t i = 0; i < moduleSize - patternSize; i++) {
            bool found = true;
            for (size_t j = 0; j < patternSize; j++) {
                uint8_t currentByte;
                if (!Read(moduleBase + i + j, currentByte)) {
                    found = false;
                    break;
                }
                if (currentByte != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return moduleBase + i;
            }
        }
        
        return 0;
    }
};
inline MemoryUtils Mem;