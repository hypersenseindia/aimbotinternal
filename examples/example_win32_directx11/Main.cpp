#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <D3DX11tex.h>
#include <d3d11.h>
#include <windows.h>
#pragma comment(lib, "D3DX11.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")
#include <thread>
#include <atomic>
#include <string>
#include <iostream>

#include "examples/example_win32_directx11/ext/MinHook/include/MinHook.h"
#include <TlHelp32.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <VersionHelpers.h>
#include <winver.h>
#pragma comment(lib, "Version.lib")
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#include <sstream>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <sddl.h>
#pragma comment(lib, "advapi32.lib")

#include <examples/example_win32_directx11/src/adb/adb.hpp>
#include <examples/example_win32_directx11/src/Overlay/Overlay.hpp>
#include <examples/example_win32_directx11/src/ui/YorzenInterface.hpp>
#include <examples/example_win32_directx11/src/Backend/SyncGlobals.hpp>
#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Data/Data.hpp>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>
#include <examples/example_win32_directx11/EspLines/Offsets.hpp>
#include <examples/example_win32_directx11/EspLines/Visuals/Visual.hpp>
#include <examples/example_win32_directx11/EspLines/Loot/LootVisual.hpp>
#include "src/Overlay/Render.hpp"
#include <examples/example_win32_directx11/src/Fonts/Fonts.hpp>
#include <imgui_settings.h>

using namespace adb;

HMODULE g_hModule = nullptr;
bool bShouldUnload = false;
FWork::Interface* g_pInterface = nullptr;

std::atomic<bool> g_AdbReady{ false };
std::atomic<bool> g_AdbFailed{ false };
std::atomic<bool> g_AuthStarted{ false };
std::atomic<bool> g_AuthDone{ false };
std::atomic<bool> g_AuthOK{ false };

static void ApplyEmulatorPerformanceMode()
{
    static bool s_lastPerf = false;
    const bool perf = g_Globals.General.DisableAllEffects;
    if (perf == s_lastPerf)
        return;
    s_lastPerf = perf;
    SetPriorityClass(GetCurrentProcess(), perf ? BELOW_NORMAL_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
}

DWORD WINAPI Unload()
{
    adb::KillEmulatorAndAdbOnExit();

    if (g_pInterface) {
        g_pInterface->ShutDown();
        g_pInterface = nullptr;
    }

    if (MemoryUtils::ogPhysRead) {
        MH_DisableHook((LPVOID)MemoryUtils::ogPhysRead);
        MH_RemoveHook((LPVOID)MemoryUtils::ogPhysRead);
    }

    MH_Uninitialize();
    bShouldUnload = true;

    if (g_hModule)
        FreeLibraryAndExitThread(g_hModule, 0);

    return 0;
}

bool MemoryInit = false;

void Memory()
{
    auto vmm = GetModuleHandleA("BstkVMM.dll");
    if (vmm == nullptr)
        return;

    auto readFunc = (MemoryUtils::PGMPhysReadFunc)GetProcAddress(vmm, "PGMPhysRead");
    if (readFunc == nullptr)
        return;

    MH_Initialize();

    if (MH_CreateHook((LPVOID)readFunc, MemoryUtils::HookedPGMPhysRead, (LPVOID*)&MemoryUtils::ogPhysRead) != MH_OK)
        return;

    if (MH_EnableHook((LPVOID)readFunc) != MH_OK)
        return;

    while (MemoryUtils::vmPtr == nullptr)
        Sleep(10);

    MemoryUtils::ogCPU = (MemoryUtils::VMMGetCpuByIdFunc)GetProcAddress(vmm, "VMMGetCpuById");
    MemoryUtils::ogCast = (MemoryUtils::PGMPhysGCPtr2GCPhysFunc)GetProcAddress(vmm, "PGMPhysGCPtr2GCPhys");
    MemoryUtils::ogWrite = (MemoryUtils::PGMPhysSimpleWriteGCPhysFunc)GetProcAddress(vmm, "PGMPhysSimpleWriteGCPhys");

    if (!MemoryUtils::ogCPU || !MemoryUtils::ogCast || !MemoryUtils::ogWrite)
        return;

    MemoryUtils::Initialize(MemoryUtils::vmPtr);
    MemoryInit = true;
}

void adbInit()
{
    TerminateAdbProcesses();

    if (!ChangeDirectory(GetExecutableDirectory())) {
        g_AdbFailed = true;
        return;
    }

    if (!adb::EnsureAdbConnection("5555")) {
        g_AdbFailed = true;
        return;
    }

    std::string il2cppStr = ExecuteShellCommandNoSu(
        "cat /proc/$(pidof com.dts.freefireth)/maps | grep libil2cpp.so");

    if (il2cppStr.empty()) {
        g_AdbFailed = true;
        return;
    }

    Offsets::Il2Cpp = ConvertToUintPtr(il2cppStr);
    if (Offsets::Il2Cpp == 0) {
        g_AdbFailed = true;
        return;
    }

    g_AdbReady = true;
}

void authInit()
{
    if (g_AuthStarted.exchange(true))
        return;

    while (!g_AuthDone.load())
        Sleep(50);

    g_AuthOK = g_AuthOK.load();
    g_AuthDone = true;
}

namespace Cheat {

void Initialize()
{
    Memory();
    if (!MemoryInit)
        MessageBoxW(NULL, L"Error Initialize Memory", L"Error", NULL);

    std::thread([] { authInit(); }).detach();

    FWork::Overlay::Setup(Render::FindRenderWindow());
    FWork::Overlay::Initialize();

    if (!FWork::Overlay::IsInitialized())
        return;

    if (!FWork::Overlay::dxGetDevice() || !FWork::Overlay::GetOverlayWindow())
        return;

    FWork::Interface Interface(
        FWork::Overlay::GetOverlayWindow(),
        FWork::Overlay::GetTargetWindow(),
        FWork::Overlay::dxGetDevice(),
        FWork::Overlay::dxGetDeviceContext());

    g_pInterface = &Interface;

    FWork::Overlay::SetupWindowProcHook(std::bind(
        &FWork::Interface::WindowProc, &Interface,
        std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4));

    MSG Message{};
    while (Message.message != WM_QUIT)
    {
        HWND hWindow = FWork::Overlay::GetOverlayWindow();
        if (hWindow == nullptr)
            break;

        if (PeekMessage(&Message, hWindow, NULL, NULL, PM_REMOVE)) {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        if (ImGui::GetCurrentContext())
            ImGui::GetIO().MouseDrawCursor = Interface.GetMenuOpen();

        if (Interface.ResizeHeight != 0 || Interface.ResizeWidht != 0) {
            FWork::Overlay::dxCleanupRenderTarget();
            if (IDXGISwapChain* pSwapChain = FWork::Overlay::dxGetSwapChain()) {
                pSwapChain->ResizeBuffers(0, Interface.ResizeWidht, Interface.ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                Interface.ResizeHeight = Interface.ResizeWidht = 0;
                FWork::Overlay::dxCreateRenderTarget();
            }
        }

        Interface.HandleMenuKey();
        FWork::Overlay::UpdateWindowPos();
        ApplyEmulatorPerformanceMode();

        if (g_Globals.EspConfig.Width <= 0 || g_Globals.EspConfig.Height <= 0 ||
            IsIconic(FWork::Overlay::GetTargetWindow())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        static bool CaptureBypassOn = false;
        if (g_Globals.General.Capture != CaptureBypassOn) {
            CaptureBypassOn = g_Globals.General.Capture;
            SetWindowDisplayAffinity(FWork::Overlay::GetOverlayWindow(),
                CaptureBypassOn ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            ImGuiIO& ioFrame = ImGui::GetIO();
            ioFrame.DeltaTime = ImMin(ioFrame.DeltaTime, 1.0f / 36.0f);
        }

        SyncUIToGlobals();

        if (MemoryInit)
            FWork::Data::Work();

        if (g_Globals.Visuals.Enabled)
            ESP::Players();

        if (g_Globals.Loot.Enabled)
            ESP::Loot();

        Interface.RenderGui();

        if (g_Globals.Misc.ShowAimbotFov && g_Globals.AimBot.Fov > 0.f &&
            g_Globals.EspConfig.Width > 0 && g_Globals.EspConfig.Height > 0) {
            const ImColor fovColor(
                g_Globals.Misc.AimbotFovColor[0],
                g_Globals.Misc.AimbotFovColor[1],
                g_Globals.Misc.AimbotFovColor[2],
                g_Globals.Misc.AimbotFovColor[3]);

            ImDrawList* fovDraw = ImGui::GetForegroundDrawList();
            fovDraw->AddCircle(
                ImVec2(g_Globals.EspConfig.Width * 0.5f, g_Globals.EspConfig.Height * 0.5f),
                g_Globals.AimBot.Fov, fovColor, 64, 1.5f);
        }

        ImGui::EndFrame();
        ImGui::Render();
        FWork::Overlay::dxRefresh();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (IDXGISwapChain* pSwapChain = FWork::Overlay::dxGetSwapChain())
            pSwapChain->Present(1, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (g_Globals.General.ShutDown) {
            Unload();
            return;
        }
    }
}

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif

    Cheat::Initialize();

    while (!bShouldUnload && !g_Globals.General.ShutDown)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
            Sleep(150);
            wWinMain((HINSTANCE)param, nullptr, nullptr, SW_SHOW);
            return 0;
        }, hModule, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        adb::KillEmulatorAndAdbOnExit();
        bShouldUnload = true;
        break;
    }
    return TRUE;
}
