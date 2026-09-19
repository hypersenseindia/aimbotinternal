#pragma once
#include <functional>
#include <atomic>

extern std::atomic<bool> g_AdbReady;
extern std::atomic<bool> g_AdbFailed;

void Backend_StartUtilityThread();
void Backend_SyncKeybindsFromYorzen();
void Backend_SyncEspPreview();
void Backend_RenderNotifications();
void Backend_Notify(const char* message, bool success = true);

void Backend_RunTempCleaner();
void Backend_ExitPanel();

void Backend_RunAdbInit();

void RAMZIXMemOnCheckboxToggled(int checkboxId, const char* label,
    const std::function<void()>& onEnable, const std::function<void()>& onDisable);

void RAMZIXMemRunLoad(const char* label, const std::function<void()>& onLoad);

bool& Backend_RAMZIXCheckbox(int id);
