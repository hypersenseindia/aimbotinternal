#include "Data.hpp"
#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>
#include <examples/example_win32_directx11/EspLines/Offsets.hpp>

#include <examples/example_win32_directx11/EspLines/Math/TMatrix.hpp>
#include <examples/example_win32_directx11/EspLines/Visuals/RankEsp.hpp>
#include <examples/example_win32_directx11/EspLines/Visuals/Namegun.h>
#include <examples/example_win32_directx11/EspLines/Loot/LootScanner.hpp>
#include <examples/example_win32_directx11/EspLines/Loot/LootDatabase.hpp>
#include <map>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <iostream>
#include <cmath>
#include <Windows.h>
#include <examples/example_win32_directx11/EspLines/Math/Vector/Vector2.hpp>
#include <examples/example_win32_directx11/EspLines/Math/Vector/Vector3.hpp>
#include <examples/example_win32_directx11/EspLines/Math/Quaternion.hpp>
#include <examples/example_win32_directx11/EspLines/Math/WordToScreen.hpp>
#include <examples/example_win32_directx11/EspLines/Math/AimB.hpp> 
#include <limits>
#include <cstdio>

#ifdef min
#undef min
#endif
#include <algorithm>
#include <cctype>
#include <string>
#ifdef max
#undef max
#endif
#include <random>
#include <intrin.h>

#include <imgui.h>
#include <imgui_settings.h>
#include <Windows.h>
#include "AimExternal.hpp"

namespace {

static short NormalizeWeaponIdShort(short weaponId) {
    if (weaponId == 0)
        return 0;
    int v = static_cast<int>(weaponId);
    if (v < 0)
        v += 25000;
    return static_cast<short>(LootDatabase::NormalizeItemId(static_cast<uint32_t>(v)));
}

static short NormalizeWeaponIdRaw(uint32_t rawId) {
    if (rawId == 0)
        return 0;
    return static_cast<short>(LootDatabase::NormalizeItemId(rawId));
}

static bool IsSniperWeaponId(short weaponId);
static short PickBestWeaponId(short a, short b);

static short ReadWeaponIdFromObject(uint32_t weaponObj) {
    if (weaponObj == 0)
        return 0;

    short best = 0;

    const uint32_t itemId = Mem.ReadS<uint32_t>(weaponObj + Offsets::Weapon_ItemId);
    best = PickBestWeaponId(best, NormalizeWeaponIdRaw(itemId));

    const uint32_t weaponData = Mem.ReadS<uint32_t>(weaponObj + Offsets::WeaponData);
    if (weaponData != 0) {
        static const uintptr_t kDataIdOffsets[] = { 0x10, 0x14, 0x18, 0x1C, 0x08, 0x0C, 0x20 };
        for (uintptr_t off : kDataIdOffsets) {
            const uint32_t v = Mem.ReadS<uint32_t>(weaponData + off);
            best = PickBestWeaponId(best, NormalizeWeaponIdRaw(v));
            const short s = Mem.ReadS<short>(weaponData + off);
            best = PickBestWeaponId(best, NormalizeWeaponIdShort(s));
        }
    }

    return best;
}

// Obsidian-internal path: Player_Data -> poolObj -> pool (+0x20 CS, +0x10 BR) -> id @ +0x10.
static short ReadWeaponIdClassicDataPool(uint32_t entity) {
    if (entity == 0)
        return 0;

    const uint32_t dataPool = Mem.ReadS<uint32_t>(entity + Offsets::Player_Data);
    if (dataPool == 0)
        return 0;

    const uint32_t poolObj = Mem.ReadS<uint32_t>(dataPool + 0x8);
    if (poolObj == 0)
        return 0;

    static const uintptr_t kPoolChainOffsets[] = { 0x20, 0x10, 0x18, 0x28, 0x24 };
    static const uintptr_t kWeaponIdOffsets[] = { 0x10, 0x14, 0x0C, 0x18, 0x24 };

    short best = 0;
    for (uintptr_t poolOff : kPoolChainOffsets) {
        const uint32_t pool = Mem.ReadS<uint32_t>(poolObj + poolOff);
        if (pool == 0)
            continue;

        for (uintptr_t idOff : kWeaponIdOffsets) {
            const uint32_t raw32 = Mem.ReadS<uint32_t>(pool + idOff);
            best = PickBestWeaponId(best, NormalizeWeaponIdRaw(raw32));
            const short raw16 = Mem.ReadS<short>(pool + idOff);
            best = PickBestWeaponId(best, NormalizeWeaponIdShort(raw16));
        }
    }

    return best;
}

static short PickBestWeaponId(short a, short b) {
    if (a == 0) return b;
    if (b == 0) return a;
    if (IsSniperWeaponId(a)) return a;
    if (IsSniperWeaponId(b)) return b;
    return a;
}

// Ob53: Player::ActiveUISightingWeapon -> GPBDEDFKJNA::DFLDCELAEMM (item id).
short ReadEquippedWeaponId(uint32_t entity) {
    if (entity == 0)
        return 0;

    short gun = 0;

    // Data pool first — matches working Obsidian Internal BR sniper detection (AWM / AWM-Y).
    gun = PickBestWeaponId(gun, ReadWeaponIdClassicDataPool(entity));
    gun = PickBestWeaponId(gun, ReadWeaponIdFromObject(Mem.ReadS<uint32_t>(entity + Offsets::Weapon)));
    gun = PickBestWeaponId(gun, ReadWeaponIdFromObject(Mem.ReadS<uint32_t>(entity + Offsets::Player_SecondaryWeapon)));

    return gun;
}

static bool IsSniperWeaponId(short weaponId) {
    weaponId = NormalizeWeaponIdShort(weaponId);
    return (weaponId == 4 || weaponId == 65 || weaponId == 21 || weaponId == 64 ||
        weaponId == 128 || weaponId == 129 || weaponId == 45 || weaponId == 75 ||
        weaponId == 78 || weaponId == 197);
}

static uint32_t ReadLocalActiveWeapon(uint32_t localPlayer) {
    if (localPlayer == 0)
        return 0;
    return Mem.ReadS<uint32_t>(localPlayer + Offsets::Weapon);
}

static bool ReadWeaponIsSighting(uint32_t weaponObj) {
    if (weaponObj == 0)
        return false;
    return Mem.ReadS<bool>(weaponObj + Offsets::Weapon_IsSighting);
}

// GAFGBLFCKAF lives on GPBDEDFKJNA (weapon instance), not on Player.
static bool ReadLocalWeaponIsSighting(uint32_t localPlayer) {
    const uint32_t activeWeapon = ReadLocalActiveWeapon(localPlayer);
    if (ReadWeaponIsSighting(activeWeapon))
        return true;

    const uint32_t bagWeapon = Mem.ReadS<uint32_t>(localPlayer + Offsets::Player_SecondaryWeapon);
    if (bagWeapon != 0 && bagWeapon != activeWeapon && ReadWeaponIsSighting(bagWeapon))
        return true;

    // BR: ADS often registers on RMB before the weapon sighting bool flips.
    return (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
}

static bool ReadLocalWeaponIsFiring(uint32_t localPlayer) {
    if (localPlayer == 0)
        return false;
    return Mem.ReadS<bool>(localPlayer + Offsets::LocalPlayerIsFiring);
}

static void ApplySniperColliderLock(uint32_t targetAddr) {
    if (targetAddr == 0)
        return;
    const uint32_t headCollider = Mem.ReadS<uint32_t>(targetAddr + Offsets::Collider);
    if (headCollider != 0)
        Mem.Write<uint32_t>(targetAddr + Offsets::LockedAimingCollider, headCollider);
}

// ESP refresh (Mirror AotForms): shared retry map + manual/auto NoCache refresh.
static std::map<uint32_t, int> s_espEntityRetryCount;
static uint32_t s_espWorkLastLocalPlayer = 0;
static std::chrono::steady_clock::time_point s_lastAutoRefreshEsp = std::chrono::steady_clock::now();

static void ApplyEspNoCacheRefresh() {
    // Mirror AotForms NoCache(): clear phys cache + entities only (no sleep — Data::Work runs off render thread).
    FWork::Data::ResetEspCache();
    s_espEntityRetryCount.clear();
    s_espWorkLastLocalPlayer = 0;
    g_Globals.EspConfig.visibleEntityCount = 0;
    g_Globals.EspConfig.previousCount = 0;
    g_Globals.EspConfig.Refresh = false;
}

static bool ReadEspBonePosition(uint32_t entity, uint32_t boneOffset, Vector3& out) {
    uint32_t bonePtr = 0;
    if (!Mem.Read<uint32_t>(entity + boneOffset, bonePtr) || bonePtr == 0)
        return false;
    return TransformUtils::GetNodePosition(bonePtr, out);
}

static void FastRefreshEspLiteStats(Player& player, uint32_t entityId) {
    player.Gun = ReadEquippedWeaponId(entityId);

    uint32_t dataPool = Mem.ReadS<uint32_t>(entityId + Offsets::Player_Data);
    if (dataPool != 0) {
        uint32_t poolObj = Mem.ReadS<uint32_t>(dataPool + 0x8);
        if (poolObj != 0) {
            uint32_t pool = Mem.ReadS<uint32_t>(poolObj + 0x10);
            if (pool != 0)
                player.Health = Mem.ReadS<short>(pool + 0x10);
        }
    }
}

static void FastRefreshEspPositions(const Vector3& mainPos) {
    for (auto& pair : g_Globals.EspConfig.Entities) {
        Player& player = pair.second;
        if (!player.IsKnown || player.IsDead)
            continue;

        FastRefreshEspLiteStats(player, pair.first);

        const Vector3 prevHead = player.Head;
        if (ReadEspBonePosition(pair.first, Offsets::Bones::Head, player.Head)) {
            if (prevHead.X != 0.f || prevHead.Y != 0.f || prevHead.Z != 0.f)
                player.Velocity = (player.Head - prevHead) * 60.0f;
            player.LastHead = player.Head;
            player.Distance = Vector3::Distance(mainPos, player.Head);
        }

        ReadEspBonePosition(pair.first, Offsets::Bones::Root, player.Root);
        ReadEspBonePosition(pair.first, Offsets::Bones::Neck, player.Neck);
        ReadEspBonePosition(pair.first, Offsets::Bones::Hip, player.Hip);

        if (!g_Globals.Visuals.Skeleton)
            continue;

        ReadEspBonePosition(pair.first, Offsets::Bones::LeftShoulder, player.LeftShoulder);
        ReadEspBonePosition(pair.first, Offsets::Bones::RightShoulder, player.RightShoulder);
        ReadEspBonePosition(pair.first, Offsets::Bones::LeftElbow, player.LeftElbow);
        ReadEspBonePosition(pair.first, Offsets::Bones::RightElbow, player.RightElbow);
        ReadEspBonePosition(pair.first, Offsets::Bones::LeftWrist, player.LeftWrist);
        ReadEspBonePosition(pair.first, Offsets::Bones::RightWrist, player.RightWrist);
        ReadEspBonePosition(pair.first, Offsets::Bones::LeftHand, player.LeftHand);
        ReadEspBonePosition(pair.first, Offsets::Bones::RightHand, player.RightHand);
        ReadEspBonePosition(pair.first, Offsets::Bones::Groin, player.Groin);
        ReadEspBonePosition(pair.first, Offsets::Bones::LeftAnkle, player.LeftAnkle);
        ReadEspBonePosition(pair.first, Offsets::Bones::RightAnkle, player.RightAnkle);
        ReadEspBonePosition(pair.first, Offsets::Bones::LeftFoot, player.LeftFoot);
        ReadEspBonePosition(pair.first, Offsets::Bones::RightFoot, player.RightFoot);
    }
}

} // namespace

void FWork::Data::ResetEspCache() {
    FWork::PullEnemy360Cpp::Stop();
    Mem.Cache.clear();
    g_Globals.EspConfig.Entities.clear();
    g_Globals.GameMode.MatchStatus = 0;
    g_Globals.Loot.GroundLoot.clear();
}

// ======================================================================
// DownPlayer - C++ port of C# DownPlayer class
// Runs its own background thread + teleport-task loop, exactly like C#.
// Use DownPlayer::Start() / DownPlayer::Stop() to control.
// ======================================================================
namespace DownPlayer
{
    static constexpr uint32_t   kDownPlayerMatrixPosOffset = 0x60; // Updated matrix position offset

    // --- State (mirrors C# private statics) ---
    static std::atomic<bool>    _isRunning{ false };
    static std::thread          _workThread;
    static std::thread          _taskThread;
    static std::atomic<bool>    _taskRunning{ false };

    static bool                 isFrozen     = false;
    static Vector3              frozenPos    = Vector3::Zero();
    static Vector3              originalPos  = Vector3::Zero();
    static uint32_t             lastMatrixPtr = 0;

    static float                teleportDownDistance = 0.9f;  // default matches C#
    static int                  teleportDelay        = 1;     // ms, matches C#

    // --- Setters (mirrors C# SetTeleportDown / SetTeleportDelay) ---
    inline void SetTeleportDown(float distance) { teleportDownDistance = distance; }
    inline void SetTeleportDelay(int delay)     { teleportDelay = delay; }

    // --- Management thread (mirrors C# Work()) ---
    static void Work()
    {
        while (_isRunning.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    // --- Teleport task loop (mirrors C# StartTeleportTask async loop) ---
    static void TeleportTaskLoop()
    {
        _taskRunning = true;

        while (_taskRunning.load())
        {
            if (!g_Globals.Misc.DownPlayer)
            {
                // Feature disabled
                if (isFrozen)
                {
                    try
                    {
                        // RE-REFRESH pointers for robust restoration (mirrors C# "RE-REFRESH Pointers" comment)
                        uint32_t localPlayer    = g_Globals.EspConfig.LocalPlayer;
                        uint32_t currentMatrixPtr = lastMatrixPtr;

                        if (localPlayer != 0)
                        {
                            uint32_t r = 0, t1 = 0, t2 = 0, m = 0;
                            if (Mem.Read(localPlayer + (uint32_t)Offsets::Bones::Root, r) && r != 0 &&
                                Mem.Read(r  + 0x8, t1) && t1 != 0 &&
                                Mem.Read(t1 + 0x8, t2) && t2 != 0 &&
                                Mem.Read(t2 + 0x20, m) && m  != 0)
                            {
                                currentMatrixPtr = m;
                            }
                        }

                        // Restoration loop: write original position 20 times with 10ms delay
                        // (200ms total) â€“ mirrors C# "for (int k = 0; k < 20; k++) { Write; await Task.Delay(10); }"
                        float opLen = sqrtf(originalPos.X * originalPos.X +
                                            originalPos.Y * originalPos.Y +
                                            originalPos.Z * originalPos.Z);
                        if (currentMatrixPtr != 0 &&
                            originalPos != Vector3::Zero() &&
                            opLen > 0.1f)
                        {
                            for (int k = 0; k < 20; k++)
                            {
                                Mem.Write<Vector3>(currentMatrixPtr + kDownPlayerMatrixPosOffset, originalPos);
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                        }
                    }
                    catch (...) {}

                    // finally block equivalent
                    isFrozen      = false;
                    lastMatrixPtr = 0;
                    originalPos   = Vector3::Zero();
                }

                // await Task.Delay(100) equivalent
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Feature enabled
            try
            {
                uint32_t localPlayer = g_Globals.EspConfig.LocalPlayer;
                if (localPlayer == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                uint32_t rootPtr = 0;
                if (!Mem.Read(localPlayer + (uint32_t)Offsets::Bones::Root, rootPtr) || rootPtr == 0)
                    goto dp_next_tick;

                uint32_t dp_t1 = 0;
                if (!Mem.Read(rootPtr + 0x8, dp_t1) || dp_t1 == 0)
                    goto dp_next_tick;

                uint32_t dp_t2 = 0;
                if (!Mem.Read(dp_t1 + 0x8, dp_t2) || dp_t2 == 0)
                    goto dp_next_tick;

                uint32_t matrixPtr = 0;
                if (!Mem.Read(dp_t2 + 0x20, matrixPtr) || matrixPtr == 0)
                    goto dp_next_tick;

                lastMatrixPtr = matrixPtr;

                {
                    Vector3 currentPos;
                    if (!Mem.Read<Vector3>(matrixPtr + kDownPlayerMatrixPosOffset, currentPos))
                        goto dp_next_tick;

                    if (!isFrozen)
                    {
                        // Capture original position (especially Y height)
                        originalPos = currentPos;

                        // Ensure originalPos is valid before moving down
                        float len = sqrtf(originalPos.X * originalPos.X +
                                          originalPos.Y * originalPos.Y +
                                          originalPos.Z * originalPos.Z);
                        if (len > 0.1f)
                        {
                            Vector3 newPos = currentPos;
                            newPos.Y -= teleportDownDistance;

                            // Apply down position
                            Mem.Write<Vector3>(matrixPtr + kDownPlayerMatrixPosOffset, newPos);
                            frozenPos = newPos;
                            isFrozen  = true;
                        }
                    }
                    else
                    {
                        // Maintain down position every tick
                        Mem.Write<Vector3>(matrixPtr + kDownPlayerMatrixPosOffset, frozenPos);
                    }
                }
            }
            catch (...) {}

        dp_next_tick:
            // await Task.Delay(teleportDelay, cts.Token) equivalent
            std::this_thread::sleep_for(std::chrono::milliseconds(teleportDelay));
        }

        _taskRunning = false;
    }

    // --- Start (mirrors C# Start()) ---
    inline void Start()
    {
        if (_isRunning.load()) return;

        _isRunning             = true;
        g_Globals.Misc.DownPlayer = true;

        // Equivalent of C# _DownPlayerThread = new Thread(Work) { IsBackground = true }; _DownPlayerThread.Start();
        _workThread = std::thread(Work);
        _workThread.detach();

        // Equivalent of C# StartTeleportTask()
        _taskThread = std::thread(TeleportTaskLoop);
        _taskThread.detach();
    }

    // --- Stop (mirrors C# Stop() â€“ does NOT cancel task, lets loop restore position) ---
    inline void Stop()
    {
        _isRunning             = false;
        g_Globals.Misc.DownPlayer = false;
        // Do NOT stop _taskRunning here â€“ the task loop detects !DownPlayer
        // and handles position restoration itself, then idles (same as C# Stop())
    }
}
// ======================================================================

// ======================================================================
// Invalid Match Timer (C# TimerManager port)
// ======================================================================

static float TimerLerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static ImVec4 TimerLerpVec4(const ImVec4& a, const ImVec4& b, float t)
{
    return ImVec4(
        TimerLerpFloat(a.x, b.x, t),
        TimerLerpFloat(a.y, b.y, t),
        TimerLerpFloat(a.z, b.z, t),
        TimerLerpFloat(a.w, b.w, t)
    );
}

class TimerManager
{
public:
    void Update(uint32_t currentMatch, uint32_t matchStatus, bool featureEnabled)
    {
        if (!featureEnabled)
        {
            StopTimer();
            return;
        }

        const bool inActiveMatch = (currentMatch != 0 && matchStatus == 1);

        if (!inActiveMatch)
        {
            const auto now = std::chrono::steady_clock::now();
            if (!inactivePending)
            {
                inactivePending = true;
                inactiveSince = now;
            }
            else if (std::chrono::duration_cast<std::chrono::seconds>(now - inactiveSince).count() >= 2)
            {
                EndMatchSession();
            }
            return;
        }

        inactivePending = false;

        // New match instance (new pointer) â€” start a fresh 3:00 countdown.
        if (currentMatch != lastMatchAddress)
        {
            lastMatchAddress = currentMatch;
            StartTimer();
            return;
        }

        if (!isRunning)
            return;

        if (!timerFinished && GetRemainingSeconds() <= 0)
            timerFinished = true;
    }

    void DrawTimer(float screenWidth, float screenHeight, bool featureEnabled)
    {
        (void)screenHeight;

        if (!featureEnabled || !isRunning || lastMatchAddress == 0)
            return;

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (!drawList)
            return;

        int remaining = GetRemainingSeconds();
        if (remaining < 0)
            remaining = 0;

        if (remaining == 0)
            timerFinished = true;

        const float DevTextY = 80.0f;
        const float GapBelowDev = 30.0f;
        // Boldness from reference (was font->Scale); use explicit size â€” avoids mutating ImFont
        const float TextScale = 1.20f;

        char buffer[64] = {};
        if (!timerFinished)
        {
            const int mm = remaining / 60;
            const int ss = remaining % 60;
            std::snprintf(buffer, sizeof(buffer), "%d:%02d", mm, ss);
        }
        else
        {
            std::snprintf(buffer, sizeof(buffer), "VALID MATCH");
        }
        const char* text = buffer;

        ImFont* font = ImGui::GetFont();
        if (!font || !text)
            return;
        if (!font->IsLoaded())
            return;

        const float font_size = font->FontSize * TextScale;
        const ImVec2 textSize = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, text, nullptr, nullptr);

        const ImVec2 basePos(screenWidth * 0.5f, DevTextY + GapBelowDev);
        const ImVec2 textPos(basePos.x - textSize.x * 0.5f, basePos.y - 18.0f);

        // Fixed colours only (not UI / c::anim accent)
        const ImVec4 redBase(0.82f, 0.12f, 0.16f, 1.0f);
        // VALID MATCH: vivid green base + bright white sweep moving left â†’ right
        const ImVec4 greenBase(0.02f, 0.82f, 0.40f, 1.0f);
        const ImVec4 whiteHi(1.0f, 1.0f, 1.0f, 1.0f);

        const float t = static_cast<float>(ImGui::GetTime());
        const float phaseRed = std::fmod(t * 0.9f, 1.0f);
        const float phaseGreen = std::fmod(t * 1.35f, 1.0f);

        float charX = textPos.x;
        for (const char* p = text; *p; ++p)
        {
            char ch[2] = { *p, '\0' };
            const ImVec2 chSize = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, ch, ch + 1, nullptr);

            const float centerX = charX + chSize.x * 0.5f;
            const float denom = (textSize.x > 1.0f) ? textSize.x : 1.0f;
            const float rel = (centerX - textPos.x) / denom;

            const float phase = timerFinished ? phaseGreen : phaseRed;
            float g = std::fmod(rel + phase, 1.0f);
            float band = 1.0f - std::fabs(g - 0.5f) * 2.0f;
            band = std::clamp(band, 0.0f, 1.0f);

            const ImVec4 base = timerFinished ? greenBase : redBase;
            const float whiteMix = timerFinished ? (band * 0.93f) : (band * 0.82f);
            ImVec4 col = TimerLerpVec4(base, whiteHi, whiteMix);

            const ImU32 glowCol = timerFinished ? IM_COL32(0, 0, 0, 170) : IM_COL32(0, 0, 0, 220);
            const ImVec2 chPos(charX, textPos.y);

            if (timerFinished)
            {
                drawList->AddText(font, font_size, ImVec2(chPos.x - 1.0f, chPos.y), glowCol, ch);
                drawList->AddText(font, font_size, ImVec2(chPos.x + 1.0f, chPos.y), glowCol, ch);
                drawList->AddText(font, font_size, ImVec2(chPos.x, chPos.y - 1.0f), glowCol, ch);
                drawList->AddText(font, font_size, ImVec2(chPos.x, chPos.y + 1.0f), glowCol, ch);
            }
            drawList->AddText(font, font_size, ImVec2(chPos.x + 1.0f, chPos.y + 1.0f), glowCol, ch);

            const ImU32 mainCol = ImGui::GetColorU32(col);
            drawList->AddText(font, font_size, chPos, mainCol, ch);

            charX += chSize.x;
        }
    }

    void StopForReset(bool featureEnabled)
    {
        // ESP entity reset must not restart the invalid-match timer.
        // Match end is detected in Update() with debouncing.
        if (!featureEnabled)
            StopTimer();
    }

private:
    void StartTimer()
    {
        startTime = std::chrono::steady_clock::now();
        isRunning = true;
        timerFinished = false;
        inactivePending = false;
    }

    void EndMatchSession()
    {
        isRunning = false;
        timerFinished = false;
        lastMatchAddress = 0;
        inactivePending = false;
    }

    void StopTimer()
    {
        EndMatchSession();
    }

    int GetRemainingSeconds() const
    {
        if (!isRunning)
            return TimerDurationSeconds;
        if (timerFinished)
            return 0;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        return TimerDurationSeconds - static_cast<int>(elapsed);
    }

    bool isRunning = false;
    bool timerFinished = false;
    bool inactivePending = false;
    uint32_t lastMatchAddress = 0;
    std::chrono::steady_clock::time_point startTime{};
    std::chrono::steady_clock::time_point inactiveSince{};

    static constexpr int TimerDurationSeconds = 180; // 3 minutes
};

static TimerManager g_TimerManager;


static uint32_t g_speedTimerLastPtr = 0;

static void ApplySpeedTimer(uint32_t currentGame)
{
    static constexpr float kSpeedOff = 0.033000f;
    static constexpr float kSpeedOn = 0.055000f;

    if (!g_Globals.Misc.SpeedTimerEnabled) {
        if (g_speedTimerLastPtr != 0) {
            Mem.Write<float>(g_speedTimerLastPtr + Offsets::FixedDeltaTime, kSpeedOff);
            g_speedTimerLastPtr = 0;
        }
        return;
    }

    uint32_t speedTimer = 0;
    if (!Mem.Read(currentGame + Offsets::GameTimer, speedTimer) || speedTimer == 0)
        return;

    g_speedTimerLastPtr = speedTimer;
    Mem.Write<float>(speedTimer + Offsets::FixedDeltaTime, kSpeedOn);
}

static void RestoreSpeedTimer()
{
    if (g_speedTimerLastPtr != 0) {
        Mem.Write<float>(g_speedTimerLastPtr + Offsets::FixedDeltaTime, 0.033000f);
        g_speedTimerLastPtr = 0;
    }
}

namespace FWork {

    // Silent Aim Max + Silent Aim Body — C# SilentAim.cs / SilentBody.cs ports
    namespace Silent_CppX {
        inline std::atomic<bool> cancel = false;
        inline std::atomic<bool> running = false;
        inline std::thread silentThread;

        struct SilentEntitySnap {
            Vector3 head = Vector3::Zero();
            Vector3 hip = Vector3::Zero();
            Vector3 root = Vector3::Zero();
            bool isKnown = false;
            bool isDead = false;
            bool isKnocked = false;
        };

        inline void SpinWait() {
            _mm_pause();
        }

        inline bool SilentEnabled() {
            return g_Globals.AimBot.SilentAimMax
                || g_Globals.AimBot.SilentAimBody
                || g_Globals.AimBot.SilentAimReworked;
        }

        inline std::vector<SilentEntitySnap> SnapshotEntities() {
            std::vector<SilentEntitySnap> snapshot;
            try {
                snapshot.reserve(g_Globals.EspConfig.Entities.size());
                for (const auto& [id, entity] : g_Globals.EspConfig.Entities) {
                    snapshot.push_back({
                        entity.Head,
                        entity.Hip,
                        entity.Root,
                        entity.IsKnown,
                        entity.IsDead,
                        entity.IsKnocked
                    });
                }
            }
            catch (...) {
                snapshot.clear();
            }
            return snapshot;
        }

        inline void WriteSilentAim(uint32_t localPlayer, const Vector3& aimWorld) {
            bool isShooting = false;
            if (!Mem.Read<bool>(localPlayer + Offsets::sAim1, isShooting) || !isShooting)
                return;

            uint32_t weaponData = 0;
            if (!Mem.Read<uint32_t>(localPlayer + Offsets::sAim2, weaponData) || weaponData == 0)
                return;

            Vector3 startPos{};
            Mem.Read<Vector3>(weaponData + Offsets::sAim3, startPos);
            Mem.Write<Vector3>(weaponData + Offsets::sAim4, aimWorld - startPos);
        }

        // Max (Head) — closest head to crosshair, +0.1 Y
        inline void WorkSilentMax(const std::vector<SilentEntitySnap>& entities,
            const Matrix4x4& viewMatrix, const Vector3& mainCamera,
            int width, int height, uint32_t localPlayer, bool ignoreKnocked)
        {
            const Vector2 screenCenter(width / 2.0f, height / 2.0f);
            Vector3 targetHead = Vector3::Zero();
            float bestDist = FLT_MAX;
            bool hasTarget = false;

            for (const auto& entity : entities) {
                if (!entity.isKnown || entity.isDead || (ignoreKnocked && entity.isKnocked))
                    continue;

                const Vector2 head2D = W2S::WorldToScreen(viewMatrix, entity.head, width, height);
                const float crossDist = Vector2::Distance(screenCenter, Vector2(head2D.X, head2D.Y));
                if (crossDist < bestDist) {
                    bestDist = crossDist;
                    targetHead = entity.head;
                    hasTarget = true;
                }
                (void)Vector3::Distance(mainCamera, entity.root);
            }

            if (!hasTarget)
                return;

            Vector3 adjusted = targetHead;
            adjusted.Y += 0.1f;
            WriteSilentAim(localPlayer, adjusted);
        }

        // Body — closest hip to crosshair, no Y bias
        inline void WorkSilentBody(const std::vector<SilentEntitySnap>& entities,
            const Matrix4x4& viewMatrix, const Vector3& mainCamera,
            int width, int height, uint32_t localPlayer)
        {
            const Vector2 screenCenter(width / 2.0f, height / 2.0f);
            Vector3 targetHip = Vector3::Zero();
            float bestDist = FLT_MAX;
            bool hasTarget = false;

            for (const auto& entity : entities) {
                if (entity.isDead || entity.isKnocked)
                    continue;

                const Vector2 hip2D = W2S::WorldToScreen(viewMatrix, entity.hip, width, height);
                const float crossDist = Vector2::Distance(screenCenter, Vector2(hip2D.X, hip2D.Y));
                if (crossDist < bestDist) {
                    bestDist = crossDist;
                    targetHip = entity.hip;
                    hasTarget = true;
                }
                (void)Vector3::Distance(mainCamera, entity.root);
            }

            if (!hasTarget)
                return;

            WriteSilentAim(localPlayer, targetHip);
        }

        inline void Work() {
            while (!cancel.load(std::memory_order_acquire)) {
                try {
                    if (!SilentEnabled()) {
                        SpinWait();
                        continue;
                    }

                    const int width = g_Globals.EspConfig.Width;
                    const int height = g_Globals.EspConfig.Height;
                    if (width == -1 || height == -1 || !g_Globals.EspConfig.Matrix) {
                        SpinWait();
                        continue;
                    }

                    const Matrix4x4 viewMatrix = g_Globals.EspConfig.ViewMatrix;
                    const Vector3 mainCamera = g_Globals.EspConfig.MainCamera;
                    const uint32_t localPlayer = g_Globals.EspConfig.LocalPlayer;
                    const bool ignoreKnocked = g_Globals.AimBot.IgnoreKnocked;
                    const std::vector<SilentEntitySnap> entities = SnapshotEntities();

                    const bool silentMax = g_Globals.AimBot.SilentAimMax || g_Globals.AimBot.SilentAimReworked;
                    const bool silentBody = g_Globals.AimBot.SilentAimBody;

                    if (silentMax)
                        WorkSilentMax(entities, viewMatrix, mainCamera, width, height, localPlayer, ignoreKnocked);
                    if (silentBody)
                        WorkSilentBody(entities, viewMatrix, mainCamera, width, height, localPlayer);

                    SpinWait();
                }
                catch (...) {
                    SpinWait();
                }
            }

            running = false;
        }

        inline void StartSilent() {
            if (running.load())
                return;

            cancel = false;
            running = true;

            silentThread = std::thread([] {
                Work();
            });

            silentThread.detach();
        }

        inline void StopSilent() {
            cancel.store(true, std::memory_order_release);
            running = false;
        }
    }

    void Data::SilentAimReworkedThread() {
        Silent_CppX::StartSilent();
    }

    void Data::autofire()
    {
        static bool isRunning = false;
        if (isRunning) return;
        isRunning = true;

        std::thread([]() {
            bool isFiring = false;

            while (true) {
                if (!g_Globals.Misc.AutoFire) {
                    if (isFiring) {
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        isFiring = false;
                    }
                    Sleep(10);
                    continue;
                }

                if (ImGui::GetIO().WantCaptureMouse) {
                    if (isFiring) {
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        isFiring = false;
                    }
                    Sleep(10);
                    continue;
                }

                CURSORINFO ci = { sizeof(CURSORINFO) };
                if (GetCursorInfo(&ci)) {
                    if (ci.flags & CURSOR_SHOWING) {
                        if (isFiring) {
                            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                            isFiring = false;
                        }
                        Sleep(10);
                        continue;
                    }
                }

                try {
                    if (g_Globals.EspConfig.Width <= 0 || g_Globals.EspConfig.Height <= 0 || !g_Globals.EspConfig.Matrix) {
                        if (isFiring) {
                            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                            isFiring = false;
                        }
                        Sleep(10);
                        continue;
                    }

                    Vector2 screenCenter(g_Globals.EspConfig.Width / 2.0f, g_Globals.EspConfig.Height / 2.0f);
                    bool targetOnCrosshair = false;

                    for (auto& [_, entity] : g_Globals.EspConfig.Entities) {
                        if (!entity.IsKnown || entity.IsDead || entity.IsTeam == Bool3::True)
                            continue;

                        if ((entity.IsKnocked && g_Globals.AimBot.IgnoreKnocked) || (entity.IsBot && g_Globals.AimBot.IgnoreBots))
                            continue;

                        Vector3 targetPosition = entity.Head;
                        if (targetPosition == Vector3::Zero())
                            continue;

                        Vector2 target2D = W2S::WorldToScreen(
                            g_Globals.EspConfig.ViewMatrix,
                            targetPosition,
                            g_Globals.EspConfig.Width,
                            g_Globals.EspConfig.Height
                        );

                        if (target2D.X < 1 || target2D.Y < 1)
                            continue;

                        float dist = Vector2::Distance(screenCenter, target2D);
                        if (dist <= g_Globals.AimBot.Fov) {
                            targetOnCrosshair = true;
                            break;
                        }
                    }

                    if (targetOnCrosshair) {
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        Sleep(4);
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        Sleep(4);
                        isFiring = true;
                    }
                    else {
                        if (isFiring) {
                            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                            isFiring = false;
                        }
                        Sleep(1);
                    }
                }
                catch (...) {
                    Sleep(10);
                }
            }
        }).detach();
    }

    std::atomic<bool> startk{ false };
    std::atomic<bool> AimBotLeft{ false };
    std::atomic<bool> cancelTask{ false };

    namespace Sniper {
        static Player* sniperTarget = nullptr;
        static bool sniperAimActive = false;

        bool IsLocalPlayerHoldingSniper();
        Player* FindClosestEnemyInSniperFOV();
        void Aimbot();
    }

    namespace PullLimit {
        static float ResolveMaxVertical(float configured)
        {
            return (configured > 0.0f) ? configured : 1.0f;
        }

        // false = do not move enemy (stay at lock position); pull only if desired is within max length
        static bool IsPullAllowed(const Vector3& original, const Vector3& desired, float maxPullLength, float maxVertical)
        {
            const float maxY = ResolveMaxVertical(maxVertical);
            const float dy = desired.Y - original.Y;
            if (fabsf(dy) > maxY)
                return false;

            if (maxPullLength > 0.0f) {
                if (Vector3::Distance(original, desired) > maxPullLength)
                    return false;
            }
            return true;
        }
    } // namespace PullLimit

    // Force Aim — instant snap onto aim line while firing (LMB)
    namespace ForceAimCpp {
        static std::unordered_map<uint32_t, Vector3> s_originalRoots;
        static uint32_t s_lockedAddr = 0;
        static int s_lastForceAimMode = -1;

        static bool ResolveRootMatrix(uint32_t entityAddr, uint32_t& matrixPtr) {
            uint32_t rootBone = 0, t1 = 0, t2 = 0;
            matrixPtr = 0;
            if (!Mem.Read(entityAddr + Offsets::Bones::Root, rootBone) || rootBone == 0)
                return false;
            if (!Mem.Read(rootBone + 0x8, t1) || t1 == 0)
                return false;
            if (!Mem.Read(t1 + 0x8, t2) || t2 == 0)
                return false;
            if (!Mem.Read(t2 + 0x20, matrixPtr) || matrixPtr == 0)
                return false;
            return true;
        }

        static void WriteRootPosition(uint32_t entityAddr, const Vector3& pos) {
            uint32_t matrixPtr = 0;
            if (!ResolveRootMatrix(entityAddr, matrixPtr))
                return;
            Mem.Write<Vector3>(matrixPtr + 0x60, pos);
        }

        static void ClearLock() {
            s_lockedAddr = 0;
        }

        static Vector3 ReadLiveBone(uint32_t entityAddr, uintptr_t boneOffset)
        {
            uint32_t bonePtr = 0;
            if (!Mem.Read(entityAddr + boneOffset, bonePtr) || bonePtr == 0)
                return Vector3::Zero();
            Vector3 pos;
            if (!TransformUtils::GetNodePosition(bonePtr, pos) || pos == Vector3::Zero())
                return Vector3::Zero();
            return pos;
        }

        static void RestoreAll() {
            for (const auto& entry : s_originalRoots)
                WriteRootPosition(entry.first, entry.second);
            s_originalRoots.clear();
            ClearLock();
        }

        static Vector3 GetAimPoint(const Player& p) {
            if (p.Address == 0)
                return Vector3::Zero();

            if (g_Globals.Misc.ForceAimMode == 1) {
                Vector3 hip = ReadLiveBone(p.Address, Offsets::Bones::Hip);
                if (hip != Vector3::Zero())
                    return hip;
                return (p.Hip != Vector3::Zero()) ? p.Hip : Vector3::Zero();
            }

            Vector3 head = ReadLiveBone(p.Address, Offsets::Bones::Head);
            if (head != Vector3::Zero())
                return head;
            return (p.Head != Vector3::Zero()) ? p.Head : Vector3::Zero();
        }

        static bool IsValidTarget(const Player& p, uint32_t localAddr) {
            if (p.Address == 0 || p.Address == localAddr)
                return false;
            if (!p.IsKnown || p.IsDead || p.IsTeam == Bool3::True)
                return false;
            if (g_Globals.AimBot.IgnoreKnocked && p.IsKnocked)
                return false;
            return GetAimPoint(p) != Vector3::Zero();
        }

        static void GetSearchFovAndDistance(uint32_t localPlayer, float& screenFov, float& maxDist3D)
        {
            screenFov = g_Globals.AimBot.Fov;
            maxDist3D = g_Globals.Misc.ForceAimMaxDistance;

            // Widen FOV/range when holding a sniper so Force Aim works at AWM distance.
            if (localPlayer == 0 || !IsSniperWeaponId(ReadEquippedWeaponId(localPlayer)))
                return;

            float sniperFov = g_Globals.AimBot.Fov;
            if (!std::isfinite(sniperFov) || sniperFov < 10.0f)
                sniperFov = 500.0f;
            if (sniperFov > 3000.0f)
                sniperFov = 3000.0f;
            if (!std::isfinite(screenFov) || screenFov < sniperFov)
                screenFov = sniperFov;

            float distAim = static_cast<float>(g_Globals.AimBot.DistanceAim);
            if (distAim < 400.0f)
                distAim = 400.0f;
            if (maxDist3D < distAim)
                maxDist3D = distAim;
            if (g_Globals.Visuals.RenderDistance > 0)
                maxDist3D = std::max(maxDist3D, static_cast<float>(g_Globals.Visuals.RenderDistance));
        }

        static Player* FindTarget() {
            Player* best = nullptr;
            float bestFov = FLT_MAX;
            const float cx = g_Globals.EspConfig.Width / 2.0f;
            const float cy = g_Globals.EspConfig.Height / 2.0f;
            const uint32_t localAddr = g_Globals.EspConfig.LocalPlayer;
            float maxDist = 0.0f;
            float aimFov = 0.0f;
            GetSearchFovAndDistance(localAddr, aimFov, maxDist);

            for (auto& kv : g_Globals.EspConfig.Entities) {
                Player* p = &kv.second;
                if (!IsValidTarget(*p, localAddr))
                    continue;

                Vector3 aimPt = GetAimPoint(*p);
                Vector2 scr = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, aimPt,
                    g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
                if (scr.X < 1.0f || scr.Y < 1.0f)
                    continue;

                float dist = Vector3::Distance(g_Globals.EspConfig.MainCamera, aimPt);
                if (dist > maxDist)
                    continue;

                float fov = sqrtf((scr.X - cx) * (scr.X - cx) + (scr.Y - cy) * (scr.Y - cy));
                if (fov > aimFov || fov >= bestFov)
                    continue;

                bestFov = fov;
                best = p;
            }
            return best;
        }

        static Player* FindLockedTarget() {
            if (s_lockedAddr == 0)
                return nullptr;
            auto it = g_Globals.EspConfig.Entities.find(s_lockedAddr);
            if (it == g_Globals.EspConfig.Entities.end())
                return nullptr;
            Player* p = &it->second;
            if (!IsValidTarget(*p, g_Globals.EspConfig.LocalPlayer))
                return nullptr;
            return p;
        }

        static void Tick(uint32_t localPlayer) {
            if (!g_Globals.Misc.ForceAim) {
                if (!s_originalRoots.empty())
                    RestoreAll();
                s_lastForceAimMode = -1;
                return;
            }
            if (g_Globals.EspConfig.Width < 1 || g_Globals.EspConfig.Height < 1 ||
                !g_Globals.EspConfig.Matrix || localPlayer == 0)
                return;

            const int aimMode = (g_Globals.Misc.ForceAimMode == 1) ? 1 : 0;
            if (s_lastForceAimMode != aimMode) {
                s_lastForceAimMode = aimMode;
                if (!s_originalRoots.empty())
                    RestoreAll();
                ClearLock();
            }

            // Pull 360 uses LocalPlayerIsFiring — required for AWM/scoped taps (LMB is too short).
            const bool gameFiring = ReadLocalWeaponIsFiring(localPlayer);
            const bool lmbHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (!gameFiring && !lmbHeld) {
                if (!s_originalRoots.empty())
                    RestoreAll();
                ClearLock();
                return;
            }

            Player* target = FindLockedTarget();
            if (!target) {
                target = FindTarget();
                if (!target)
                    return;
                s_lockedAddr = target->Address;
            }

            uint32_t rootBone = 0;
            if (!Mem.Read(target->Address + Offsets::Bones::Root, rootBone) || rootBone == 0)
                return;

            Vector3 currentRoot;
            if (!TransformUtils::GetNodePosition(rootBone, currentRoot))
                return;

            if (s_originalRoots.find(target->Address) == s_originalRoots.end())
                s_originalRoots[target->Address] = currentRoot;

            const Vector3 originalRoot = s_originalRoots[target->Address];

            Vector3 aimTarget = GetAimPoint(*target);
            if (aimTarget == Vector3::Zero())
                return;

            // Keep selected bone (head or hip) on the aim line using live bone + root offset.
            const Vector3 rootOffsetFromAim = currentRoot - aimTarget;

            Vector3 fwd(g_Globals.EspConfig.ViewMatrix.m02,
                g_Globals.EspConfig.ViewMatrix.m12,
                g_Globals.EspConfig.ViewMatrix.m22);
            float len = sqrtf(fwd.X * fwd.X + fwd.Y * fwd.Y + fwd.Z * fwd.Z);
            if (len < 1e-4f)
                return;
            fwd.X /= len;
            fwd.Y /= len;
            fwd.Z /= len;

            const Vector3 cam = g_Globals.EspConfig.MainCamera;
            Vector3 toAim = aimTarget - cam;
            float proj = fwd.X * toAim.X + fwd.Y * toAim.Y + fwd.Z * toAim.Z;
            Vector3 aimOnLine = cam + fwd * proj;
            Vector3 desiredRoot = aimOnLine + rootOffsetFromAim;

            float maxPull = g_Globals.Misc.ForceAimMaxPull;
            if (maxPull <= 0.0f)
                maxPull = 8.0f;
            if (!PullLimit::IsPullAllowed(originalRoot, desiredRoot, maxPull, g_Globals.Misc.ForceAimMaxPullVertical)) {
                WriteRootPosition(target->Address, originalRoot);
                return;
            }

            WriteRootPosition(target->Address, desiredRoot);

            // Head collider swap helps AWM register head hits while pulling.
            if (IsSniperWeaponId(ReadEquippedWeaponId(localPlayer)))
                ApplySniperColliderLock(target->Address);
        }
    } // namespace ForceAimCpp

    // Enemy Pull — FOV pull onto fire line (Silent Aim X port)
    namespace PullEnemy360Cpp {
        static std::unordered_map<uint32_t, Vector3> s_originalPositions;
        static std::chrono::steady_clock::time_point s_lastTick{};

        static void RestoreAllPositions() {
            for (const auto& entry : s_originalPositions) {
                uint32_t rootBonePtr = 0, t1 = 0, t2 = 0, matrixPtr = 0;
                if (!Mem.Read(entry.first + Offsets::Bones::Root, rootBonePtr) || rootBonePtr == 0)
                    continue;
                if (!Mem.Read(rootBonePtr + 0x8, t1) || t1 == 0)
                    continue;
                if (!Mem.Read(t1 + 0x8, t2) || t2 == 0)
                    continue;
                if (!Mem.Read(t2 + 0x20, matrixPtr) || matrixPtr == 0)
                    continue;
                Mem.Write<Vector3>(matrixPtr + 0x60, entry.second);
            }
            s_originalPositions.clear();
        }

        static bool EnemyPullActive() {
            return g_Globals.Misc.EnemyPullEnabled || g_Globals.Misc.PullEnemy360Enabled;
        }

        static int TickMs() {
            const int a = g_Globals.Misc.EnemyPullTickMs;
            const int b = g_Globals.Misc.PullEnemy360TickMs;
            const int v = (a > 0) ? a : b;
            return (v > 0) ? v : 6;
        }

        static float MaxDistance() {
            const float a = g_Globals.Misc.EnemyPullMaxDistance;
            const float b = g_Globals.Misc.PullEnemy360MaxDistance;
            if (a > 1.0f) return a;
            if (b > 1.0f) return b;
            return 250.0f;
        }

        static Player* FindBestTarget() {
            Player* bestTarget = nullptr;
            float closestDist = FLT_MAX;
            const int w = g_Globals.EspConfig.Width;
            const int h = g_Globals.EspConfig.Height;
            if (w < 1 || h < 1 || !g_Globals.EspConfig.Matrix)
                return nullptr;

            const Vector2 screenCenter(w / 2.0f, h / 2.0f);
            const float aimFov = g_Globals.AimBot.Fov;
            const float maxDist3D = MaxDistance();
            const bool ignoreKnocked = g_Globals.AimBot.IgnoreKnocked;
            const uint32_t localAddr = g_Globals.EspConfig.LocalPlayer;

            for (auto& kv : g_Globals.EspConfig.Entities) {
                Player& ent = kv.second;
                if (ent.Address == 0 || ent.Address == localAddr)
                    continue;
                if (!ent.IsKnown || ent.IsDead)
                    continue;
                if (ent.IsTeam == Bool3::True)
                    continue;
                if (ignoreKnocked && ent.IsKnocked)
                    continue;

                Vector2 head2D = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, ent.Head, w, h);
                if (head2D.X < 1.0f || head2D.Y < 1.0f)
                    continue;

                float dist3D = Vector3::Distance(g_Globals.EspConfig.MainCamera, ent.Head);
                if (dist3D > maxDist3D)
                    continue;

                float crosshairDist = Vector2::Distance(screenCenter, head2D);
                if (crosshairDist > aimFov)
                    continue;

                if (crosshairDist < closestDist) {
                    closestDist = crosshairDist;
                    bestTarget = &ent;
                }
            }
            return bestTarget;
        }

        static void ApplyInstantPull(Player& entity) {
            uint32_t rootBonePtr = 0, transformValue = 0, transformObjPtr = 0, matrixPtr = 0;
            if (!Mem.Read(entity.Address + Offsets::Bones::Root, rootBonePtr) || rootBonePtr == 0)
                return;
            if (!Mem.Read(rootBonePtr + 0x8, transformValue) || transformValue == 0)
                return;
            if (!Mem.Read(transformValue + 0x8, transformObjPtr) || transformObjPtr == 0)
                return;
            if (!Mem.Read(transformObjPtr + 0x20, matrixPtr) || matrixPtr == 0)
                return;

            Vector3 currentPos;
            if (!TransformUtils::GetNodePosition(rootBonePtr, currentPos))
                return;

            if (s_originalPositions.find(entity.Address) == s_originalPositions.end())
                s_originalPositions[entity.Address] = currentPos;

            const Vector3 originalPos = s_originalPositions[entity.Address];

            Vector3 fireDir(
                g_Globals.EspConfig.ViewMatrix.m02,
                g_Globals.EspConfig.ViewMatrix.m12,
                g_Globals.EspConfig.ViewMatrix.m22);
            float len = sqrtf(fireDir.X * fireDir.X + fireDir.Y * fireDir.Y + fireDir.Z * fireDir.Z);
            if (len < 1e-4f)
                return;
            fireDir.X /= len;
            fireDir.Y /= len;
            fireDir.Z /= len;

            const Vector3 camPos = g_Globals.EspConfig.MainCamera;
            Vector3 toEnemy = currentPos - camPos;
            float projLength = toEnemy.X * fireDir.X + toEnemy.Y * fireDir.Y + toEnemy.Z * fireDir.Z;
            Vector3 finalPos = camPos + fireDir * projLength;

            constexpr float kClamp = 5.0f;
            float deltaX = finalPos.X - originalPos.X;
            float deltaZ = finalPos.Z - originalPos.Z;
            if (fabsf(deltaX) > kClamp)
                finalPos.X = originalPos.X + copysignf(kClamp, deltaX);
            if (fabsf(deltaZ) > kClamp)
                finalPos.Z = originalPos.Z + copysignf(kClamp, deltaZ);
            finalPos.Y = originalPos.Y;

            Mem.Write<Vector3>(matrixPtr + 0x60, finalPos);
        }

        static void Tick(uint32_t localPlayer) {
            if (!EnemyPullActive()) {
                if (!s_originalPositions.empty())
                    RestoreAllPositions();
                s_lastTick = std::chrono::steady_clock::now();
                return;
            }

            const int tickMs = TickMs();
            const auto now = std::chrono::steady_clock::now();
            if (now - s_lastTick < std::chrono::milliseconds(tickMs))
                return;
            s_lastTick = now;

            if (g_Globals.EspConfig.Width < 1 || g_Globals.EspConfig.Height < 1 ||
                !g_Globals.EspConfig.Matrix || localPlayer == 0)
                return;

            bool isFiring = false;
            if (!Mem.Read<bool>(localPlayer + Offsets::isFiring, isFiring) || !isFiring) {
                RestoreAllPositions();
                return;
            }

            Player* target = FindBestTarget();
            if (target != nullptr)
                ApplyInstantPull(*target);
            else
                RestoreAllPositions();
        }

        void Stop()
        {
            RestoreAllPositions();
            s_lastTick = std::chrono::steady_clock::now();
        }
    } // namespace PullEnemy360Cpp

    // TeleportMark and TeleportMap static variables removed from global scope

    Player* FindClosestEnemyInFOV();


    Player* FindClosestEnemy() {
        float closestDistance = FLT_MAX;
        Player* closestEntity = nullptr;

        Vector2 screenCenter(g_Globals.EspConfig.Width / 2.0f, g_Globals.EspConfig.Height / 2.0f);

        for (auto& pair : g_Globals.EspConfig.Entities) {
            Player* entity = &pair.second;

            if (entity->IsDead || (g_Globals.AimBot.IgnoreKnocked && entity->IsKnocked)) continue;

            ImVec2 head2D = W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, entity->Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
            if (head2D.x < 1 || head2D.y < 1) continue;

            float distance = Vector3::Distance(g_Globals.EspConfig.MainCamera, entity->Head);
            if (distance > g_Globals.AimBot.DistanceAim) continue;

            float crosshairDist = std::sqrt(std::pow(head2D.x - screenCenter.X, 2) + std::pow(head2D.y - screenCenter.Y, 2));
            if (crosshairDist >= closestDistance) continue;

            closestDistance = crosshairDist;
            closestEntity = entity;
        }

        return closestEntity;
    }

    Player* FindClosestEnemyInFOV() {
        float closestDistance = FLT_MAX;
        Player* closestEntity = nullptr;

        Vector2 screenCenter(g_Globals.EspConfig.Width / 2.0f, g_Globals.EspConfig.Height / 2.0f);

        for (auto& pair : g_Globals.EspConfig.Entities) {
            Player* entity = &pair.second;

            if (entity->IsDead || (g_Globals.AimBot.IgnoreKnocked && entity->IsKnocked) || (g_Globals.AimBot.IgnoreBots && entity->IsBot)) continue;


            ImVec2 head2D = W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, entity->Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
            if (head2D.x < 1 || head2D.y < 1) continue;

            float distance = Vector3::Distance(g_Globals.EspConfig.MainCamera, entity->Head);
            if (distance > g_Globals.AimBot.DistanceAim) continue;

            float crosshairDist = std::sqrt(std::pow(head2D.x - screenCenter.X, 2) + std::pow(head2D.y - screenCenter.Y, 2));
            if (crosshairDist > g_Globals.AimBot.Fov || crosshairDist >= closestDistance) continue;

            closestDistance = crosshairDist;
            closestEntity = entity;
        }

        return closestEntity;
    }

    bool Sniper::IsLocalPlayerHoldingSniper() {
        if (g_Globals.EspConfig.LocalPlayer == 0) return false;

        uint32_t dataPooll = Mem.ReadS<uint32_t>(g_Globals.EspConfig.LocalPlayer + Offsets::Player_Data);
        if (dataPooll == 0) return false;

        uint32_t poolObjj = Mem.ReadS<uint32_t>(dataPooll + 0x8);
        if (poolObjj == 0) return false;

        uint32_t pooll = Mem.ReadS<uint32_t>(poolObjj + 0x20);
        if (pooll == 0) return false;

        short weaponId = Mem.ReadS<short>(pooll + 0x10);

        // Sniper rifle IDs: AWM(4), AWM-Y(65), Kar98K(21), Kar98K-I(64), Kar98K-II(128), Kar98K-III(129), M82B(45), M24(75), VSK94(197)
        return (weaponId == 4 || weaponId == 65 || weaponId == 21 || weaponId == 64 ||
            weaponId == 128 || weaponId == 129 || weaponId == 45 || weaponId == 75 || weaponId == 197);
    }

    Player* Sniper::FindClosestEnemyInSniperFOV() {
        float closestDistance = FLT_MAX;
        Player* closestEntity = nullptr;

        Vector2 screenCenter(g_Globals.EspConfig.Width / 2.0f, g_Globals.EspConfig.Height / 2.0f);

        for (auto& pair : g_Globals.EspConfig.Entities) {
            Player* entity = &pair.second;

            if (entity->IsDead || (g_Globals.AimBot.IgnoreKnocked && entity->IsKnocked) || (g_Globals.AimBot.IgnoreBots && entity->IsBot)) continue;


            ImVec2 head2D = W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, entity->Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
            if (head2D.x < 1 || head2D.y < 1) continue;

            float distance = Vector3::Distance(g_Globals.EspConfig.MainCamera, entity->Head);
            if (distance > g_Globals.AimBot.DistanceAim) continue;

            float crosshairDist = std::sqrt(std::pow(head2D.x - screenCenter.X, 2) + std::pow(head2D.y - screenCenter.Y, 2));
            if (crosshairDist > g_Globals.Misc.SniperFov || crosshairDist >= closestDistance) continue;

            closestDistance = crosshairDist;
            closestEntity = entity;
        }

        return closestEntity;
    }

    void Sniper::Aimbot() {
        if (!g_Globals.Misc.SniperScope) {
            sniperAimActive = false;
            sniperTarget = nullptr;
            return;
        }

        // Only activate if local player is holding a sniper
        if (!Sniper::IsLocalPlayerHoldingSniper()) {
            sniperAimActive = false;
            sniperTarget = nullptr;
            return;
        }

        bool isFirePressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        // If fire key is pressed and we don't have a target, find one
        if (isFirePressed && !sniperAimActive) {
            Player* target = Sniper::FindClosestEnemyInSniperFOV();
            if (target && target->Address != 0) {
                sniperTarget = target;
                sniperAimActive = true;
            }
        }

        // If fire key is released, stop aiming
        if (!isFirePressed) {
            sniperAimActive = false;
            sniperTarget = nullptr;
            return;
        }

        // If we have an active target and fire is still pressed, maintain aim
        if (sniperAimActive && sniperTarget && sniperTarget->Address != 0) {
            // Check if target is still valid (not dead, still in range)
            if (sniperTarget->IsDead ||
                Vector3::Distance(g_Globals.EspConfig.MainCamera, sniperTarget->Head) > g_Globals.AimBot.DistanceAim) {
                sniperAimActive = false;
                sniperTarget = nullptr;
                return;
            }

            // Decide where to aim based on sniper scope mode
            // 0 = Head, 1 = Body (Hip)
            Vector3 aimTarget = (g_Globals.Misc.SniperScopeMode == 1)
                ? sniperTarget->Hip   // Body mode: use real hip bone
                : sniperTarget->Head; // Head mode

            auto playerLook = AimB::GetRotationToLocation(aimTarget, 0.0f, g_Globals.EspConfig.MainCamera);
            Mem.Write(g_Globals.EspConfig.LocalPlayer + Offsets::AimRotation, playerLook);

            // Keep using head collider for reliable hit registration
            uint32_t headCollider = Mem.ReadS<uint32_t>(sniperTarget->Address + Offsets::Collider);
            if (headCollider != 0) {
                Mem.Write<uint32_t>(sniperTarget->Address + Offsets::LockedAimingCollider, headCollider);
            }
        }
    }

    Player* FindBestTarget()
    {
        float closestDistance = FLT_MAX;
        Player* closestEntity = nullptr;

        ImVec2 screenCenter = ImVec2(g_Globals.EspConfig.Width / 2.0f, g_Globals.EspConfig.Height / 2.0f);

        for (auto& pair : g_Globals.EspConfig.Entities) {
            Player* entity = &pair.second;

            // Safety check - ensure entity is valid
            if (!entity || entity->Address == 0) continue;

            if (entity->IsDead || (g_Globals.AimBot.IgnoreKnocked && entity->IsKnocked)) continue;

            ImVec2 head2D = W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, entity->Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
            if (head2D.x < 1 || head2D.y < 1) continue;

            float distance = Vector3::Distance(g_Globals.EspConfig.MainCamera, entity->Head);
            if (distance > g_Globals.AimBot.DistanceAim) continue;

            float crosshairDist = std::sqrt(std::pow(head2D.x - screenCenter.x, 2) + std::pow(head2D.y - screenCenter.y, 2));
            if (crosshairDist >= closestDistance) continue;

            closestDistance = crosshairDist;
            closestEntity = entity;
        }
        return closestEntity;
    }

    void TriggerAimbot() {
        if (!g_Globals.AimBot.Rage) {
            return;
        }

        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            return;
        }

        Player* target = FindClosestEnemyInFOV();
        if (!target || target->Address == 0) {
            return;
        }

        auto playerLook = AimB::GetRotationToLocation(target->Head, 0.0f, g_Globals.EspConfig.MainCamera);
        Mem.Write(g_Globals.EspConfig.LocalPlayer + Offsets::AimRotation, playerLook);

        uint32_t headCollider = Mem.ReadS<uint32_t>(target->Address + Offsets::Collider);
        if (headCollider != 0) {
            Mem.Write<uint32_t>(target->Address + Offsets::LockedAimingCollider, headCollider);
        }
    }




    void LegitAimbot() {
        if (!g_Globals.AimBot.Enabled || !(GetAsyncKeyState(g_Globals.AimBot.KeyBind) & 0x8000)) {
            return;
        }

        Player* target = FindClosestEnemy();
        if (!target || target->Address == 0) {
            return;
        }

        uint32_t headCollider = Mem.ReadS<uint32_t>(target->Address + Offsets::Collider);
        if (headCollider == 0) {
            return;
        }

        Mem.Write<uint32_t>(target->Address + Offsets::LockedAimingCollider, headCollider);
    }


    void MouseHook_LButtonDown()
    {
        if (startk)
        {
            cancelTask = false;
            std::thread([]
                {
                    int delay = Aimbotype;
                    for (int i = 0; i < delay / 10; i++)
                    {
                        if (cancelTask) return;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    if (!cancelTask)
                        AimBotLeft = true;
                }).detach();
        }
    }

    void AimAtTarget(Player* target)
    {
        auto playerLook = AimB::GetRotationToLocation(target->Head, 0.0f, g_Globals.EspConfig.MainCamera);
        Mem.Write(g_Globals.EspConfig.LocalPlayer + Offsets::AimRotation, playerLook);
        
        if (aimlegit > 0.0f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(aimlegit)));
        }
    }

    bool IsValidWeaponForRageV2(short weaponId) {
        // Adjust negative IDs (same as Namegun::GetGunName)
        int adjustedId = weaponId < 0 ? weaponId + 25000 : weaponId;

        // Exclude fists
        if (weaponId == 1) return false;

        // Exclude grenades
        if (weaponId == 601 || weaponId == 602 || weaponId == 603 || weaponId == 608) return false;

        // Exclude special items
        if (weaponId == 99 || weaponId == 617 || weaponId == 1401 || weaponId == 1006 || weaponId == 1015) return false;
        if (weaponId == 6016 || weaponId == 10006 || weaponId == 21001 || weaponId == 21002) return false;
        if (adjustedId == 9476 || adjustedId == 9479) return false;

        // Valid weapons: Rifles
        if (weaponId == 0 || weaponId == 2 || weaponId == 6 || weaponId == 11 || weaponId == 12 || weaponId == 14 ||
            weaponId == 24 || weaponId == 28 || weaponId == 33 || weaponId == 39 || weaponId == 46 || weaponId == 47 ||
            weaponId == 57 || weaponId == 63 || weaponId == 67 || weaponId == 70 || weaponId == 73 || weaponId == 74 ||
            weaponId == 80 || weaponId == 81 || weaponId == 82 || weaponId == 87 || weaponId == 101 || weaponId == 103 ||
            weaponId == 104 || weaponId == 126 || weaponId == 127 || weaponId == 130 || weaponId == 131 ||
            weaponId == 178 || weaponId == 179 || weaponId == 180 || weaponId == 193 || weaponId == 194 ||
            weaponId == 195) return true;

        // Valid weapons: Marksman Rifles
        if (weaponId == 18 || weaponId == 26 || weaponId == 44 || weaponId == 48 || weaponId == 72 || weaponId == 89 ||
            weaponId == 102 || weaponId == 105 || weaponId == 106 || weaponId == 107) return true;

        // Valid weapons: LMG
        if (weaponId == 19 || weaponId == 30 || weaponId == 54 || weaponId == 61 || weaponId == 71 ||
            weaponId == 96 || weaponId == 122 || weaponId == 123) return true;

        // Valid weapons: SMG
        if (weaponId == 7 || weaponId == 8 || weaponId == 13 || weaponId == 15 || weaponId == 32 ||
            weaponId == 35 || weaponId == 43 || weaponId == 49 || weaponId == 60 || weaponId == 62 ||
            weaponId == 69 || weaponId == 88 || weaponId == 120 || weaponId == 121 || weaponId == 124 ||
            weaponId == 125 || weaponId == 150 || weaponId == 228 || weaponId == 229 || weaponId == 230) return true;

        // Valid weapons: Shotguns
        if (weaponId == 5 || weaponId == 29 || weaponId == 41 || weaponId == 50 || weaponId == 86 ||
            weaponId == 98 || weaponId == 119 || weaponId == 181 || weaponId == 182 || weaponId == 184 ||
            weaponId == 185 || weaponId == 186) return true;

        if (weaponId == 4 || weaponId == 21 || weaponId == 45 || weaponId == 64 || weaponId == 65 ||
            weaponId == 75 || weaponId == 78 || weaponId == 128 || weaponId == 129 || weaponId == 197) return true;

        // Valid weapons: Pistols
        if (weaponId == 3 || weaponId == 9 || weaponId == 10 || weaponId == 20 || weaponId == 25 ||
            weaponId == 55 || weaponId == 56 || weaponId == 58 || weaponId == 76 || weaponId == 93 ||
            weaponId == 137) return true;

        // Valid weapons: Melee
        if (weaponId == 16 || weaponId == 17 || weaponId == 27 || weaponId == 34 || weaponId == 51 ||
            weaponId == 53) return true;

        // Valid weapons: Others (Launchers, Flamethrower)
        if (weaponId == 23 || weaponId == 36 || weaponId == 37 || weaponId == 100 || weaponId == 136 ||
            weaponId == 138 || weaponId == 139 || weaponId == 142 || weaponId == 196) return true;

        return false;
    }

    void RageV2Aimbot() {
        if (!g_Globals.AimBot.Ragev2 || !(GetAsyncKeyState(g_Globals.AimBot.KeyBind) & 0x8000)) {
            return;
        }

        // Check if player has a valid weapon equipped
        if (g_Globals.EspConfig.LocalPlayer == 0) {
            return;
        }

        uint32_t dataPooll = Mem.ReadS<uint32_t>(g_Globals.EspConfig.LocalPlayer + Offsets::Player_Data);
        if (dataPooll == 0) {
            return;
        }

        uint32_t poolObjj = Mem.ReadS<uint32_t>(dataPooll + 0x8);
        if (poolObjj == 0) {
            return;
        }

        uint32_t pooll = Mem.ReadS<uint32_t>(poolObjj + 0x20);
        if (pooll == 0) {
            return;
        }

        short weaponId = Mem.ReadS<short>(pooll + 0x10);

        // Only work with valid weapons (not fists, grenades, or special items)
        if (!IsValidWeaponForRageV2(weaponId)) {
            return;
        }

        Player* target = FindBestTarget();
        if (!target || target->Address == 0) {
            return;
        }

        AimAtTarget(target);
    }

    void Data::Work() {
        static bool downPlayerRuntimeStarted = false;

        // Auto refresh ESP every 2 seconds (mirror AotForms ESP.cs AutoRefreshESP).
        if (g_Globals.EspConfig.AutoRefresh) {
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - s_lastAutoRefreshEsp).count() >= 2) {
                ApplyEspNoCacheRefresh();
                s_lastAutoRefreshEsp = now;
            }
        }

        // Apply manual ESP refresh immediately (before any early return) so the flag
        // never sticks, lobby/empty-count frames still clear stale state, and the next
        // in-match tick does a clean full rescan.
        if (g_Globals.EspConfig.Refresh) {
            ApplyEspNoCacheRefresh();
        }

        if (g_Globals.General.DisableAllEffects) {
            static auto s_lastCacheTrim = std::chrono::steady_clock::now();
            const auto nowTrim = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(nowTrim - s_lastCacheTrim).count() >= 30) {
                if (Mem.Cache.size() > 1024)
                    Mem.Cache.clear();
                s_lastCacheTrim = nowTrim;
            }
        }

        try {

            if (Offsets::Il2Cpp == 0) {
                //Reset();
                return;
            }

            uint32_t baseGameFacade = Mem.ReadS<uint32_t>(Offsets::Il2Cpp + Offsets::InitBase);
            if (!baseGameFacade)
            {
                ResetEspCache();
                return;
            }

            uint32_t gameFacade = Mem.ReadS<uint32_t>(baseGameFacade);
            if (!gameFacade) {
                ResetEspCache();
                return;
            }

            uint32_t staticGameFacade = Mem.ReadS<uint32_t>(gameFacade + Offsets::StaticClass);
            if (!staticGameFacade) {
                ResetEspCache();
                return;
            }

            uint32_t currentGame = Mem.ReadS<uint32_t>(staticGameFacade);
            if (!currentGame) {
                ResetEspCache();
                return;
            }

            uint32_t currentMatch = Mem.ReadS<uint32_t>(currentGame + Offsets::CurrentMatch);
            uint32_t matchStatus = 0;
            if (currentMatch)
                matchStatus = Mem.ReadS<uint32_t>(currentMatch + Offsets::MatchStatus);

            // Update before Reset() so brief lobby/loading flickers do not restart the 3:00 timer.
            g_TimerManager.Update(currentMatch, matchStatus, g_Globals.Visuals.InvalidTimer);

            if (!currentMatch) {
                ResetEspCache();
                return;
            }

            if (!matchStatus) {
                ResetEspCache();
                return;
            }

            if (matchStatus != 1) {
                ResetEspCache();
                return;
            }

            uint32_t localPlayer = Mem.ReadS<uint32_t>(currentMatch + Offsets::LocalPlayer);

            // Observer ESP Override (Spectator Fix)
            uint32_t observerState = 0;
            if (localPlayer != 0) {
                observerState = Mem.ReadS<uint32_t>(localPlayer + 0xB4);
            }
            if (observerState == 0) {
                observerState = Mem.ReadS<uint32_t>(currentMatch + 0xB4);
            }

            if (observerState != 0) {
                uint32_t spectatedPlayer = Mem.ReadS<uint32_t>(observerState + 0x28);
                if (spectatedPlayer != 0) {
                    localPlayer = spectatedPlayer;
                }
            }

            if (!localPlayer) {
                return;
            }

            // Cache local player for misc features
            g_Globals.EspConfig.LocalPlayer = localPlayer;

            // Keep DownPlayer runtime in sync with menu toggle.
            // Without this bridge, enabling from UI may not start the background loop.
            if (g_Globals.Misc.DownPlayer) {
                if (!downPlayerRuntimeStarted) {
                    DownPlayer::Start();
                    downPlayerRuntimeStarted = true;
                }
            }
            else {
                if (downPlayerRuntimeStarted) {
                    DownPlayer::Stop();
                    downPlayerRuntimeStarted = false;
                }
            }

            ApplySpeedTimer(currentGame);

            uint32_t mainTransform = Mem.ReadS<uint32_t>(localPlayer + Offsets::MainCameraTransform);
            if (!mainTransform) {
                //Reset();
                return;
            }

            Vector3 mainPos;
            TransformUtils::GetPosition(mainTransform, mainPos);
            g_Globals.EspConfig.MainCamera = mainPos;

            uint32_t followCamera = Mem.ReadS<uint32_t>(localPlayer + Offsets::FollowCamera);
            if (!followCamera) {
                //Reset();
                return;
            }

            uint32_t camera = Mem.ReadS<uint32_t>(followCamera + Offsets::Camera);
            if (!camera) {
                // Reset();
                return;
            }

            uint32_t cameraBase = Mem.ReadS<uint32_t>(camera + 0x8);
            if (!cameraBase) {
                // Reset();
                return;
            }

            Matrix4x4 viewMatrix = Mem.ReadS<Matrix4x4>(cameraBase + Offsets::ViewMatrix);
            g_Globals.EspConfig.Matrix = true;
            g_Globals.EspConfig.ViewMatrix = viewMatrix;

            if (g_Globals.Loot.Enabled && Mem.IsReady()) {
                static auto lastLootScan = std::chrono::steady_clock::now();
                const auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLootScan).count() >= 300) {
                    FWork::Loot::ScanGround(currentGame, mainPos);
                    lastLootScan = now;
                }
            }
            else if (!g_Globals.Loot.GroundLoot.empty()) {
                g_Globals.Loot.GroundLoot.clear();
                g_Globals.Loot.GroundLoot.shrink_to_fit();
            }

            static std::chrono::steady_clock::time_point s_lastEntityScan{};
            const auto entityScanNow = std::chrono::steady_clock::now();
            // Perf mode: per-frame bone refresh keeps ESP smooth; full scan stays at 33ms.
            const int kEntityScanIntervalMs = 33;
            const bool runEntityScan = (s_lastEntityScan.time_since_epoch().count() == 0)
                || (std::chrono::duration_cast<std::chrono::milliseconds>(entityScanNow - s_lastEntityScan).count() >= kEntityScanIntervalMs);

            if (runEntityScan)
                s_lastEntityScan = entityScanNow;

            uint32_t entityDictionary = Mem.ReadS<uint32_t>(currentGame + Offsets::DictionaryEntities);

            if (!entityDictionary) {
                ResetEspCache();
                return;
            }

            uint32_t entities = Mem.ReadS<uint32_t>(entityDictionary + 0xC);
            if (!entities) {
                ResetEspCache();
                return;
            }

            entities += 0x1C;

            uint32_t entitiesCount = Mem.ReadS<uint32_t>(entityDictionary + 0x10);

            bool newGameDetected = false;

            if (entitiesCount != g_Globals.EspConfig.previousCount) {
                // If entity count changed significantly or local player changed, it's a new game
                if (abs((int)entitiesCount - (int)g_Globals.EspConfig.previousCount) > 5 ||
                    s_espWorkLastLocalPlayer != localPlayer) {
                    newGameDetected = true;
                    // Clear all entities to force reload
                   // g_MiscForceAimOriginalRoot.clear();
                    g_Globals.EspConfig.Entities.clear();
                    Mem.Cache.clear(); // Clear cache for fresh reads
                }
                g_Globals.EspConfig.previousCount = entitiesCount;
                s_espWorkLastLocalPlayer = localPlayer;
            }

            if (entitiesCount < 1) {
                AimRage::AimbotMode::StartAimbot();
                RageV2Aimbot();
                LegitAimbot();
                TriggerAimbot();
                SilentAimReworkedThread();
                Sniper::Aimbot();
                return;
            }

            AimRage::AimbotMode::StartAimbot();
            // Clear retry counts when new game is detected
            if (newGameDetected) {
                s_espEntityRetryCount.clear();
            }

            if (g_Globals.AimBot.NoRecoil) {
                uint64_t weapon = Mem.ReadS<uint64_t>(localPlayer + Offsets::Weapon);
                if (weapon) {
                    uint64_t weaponData = Mem.ReadS<uint64_t>(weapon + Offsets::WeaponData);
                    if (weaponData) {
                        float recoil = Mem.ReadS<float>(weaponData + Offsets::WeaponRecoil);
                        if (recoil != 0.0f) {
                            Mem.Write<float>(weaponData + Offsets::WeaponRecoil, 0.0f);
                        }
                    }
                }
            }

            if (g_Globals.Misc.FastReload) {
                uint32_t reload = Mem.ReadS<uint32_t>(localPlayer + Offsets::LocalPlayerAttributes);
                if (reload) {
                    Mem.Write<bool>(reload + Offsets::NoReload, true);
                }
            }
            else {
                uint32_t reload = Mem.ReadS<uint32_t>(localPlayer + Offsets::LocalPlayerAttributes);
                if (reload) {
                    Mem.Write<bool>(reload + Offsets::NoReload, false);
                }
            }

            // Unlimited ammo toggle (PlayerAttributes + 0xE0)
            if (g_Globals.Misc.UnlimitedAmmo) {
                uint32_t ammoAddr = Mem.ReadS<uint32_t>(localPlayer + Offsets::PlayerAttributes);
                if (ammoAddr) {
                    Mem.Write<bool>(ammoAddr + Offsets::UnlimitedAmmo, true);
                }
            }
            else {
                uint32_t ammoAddr = Mem.ReadS<uint32_t>(localPlayer + Offsets::PlayerAttributes);
                if (ammoAddr) {
                    Mem.Write<bool>(ammoAddr + Offsets::UnlimitedAmmo, false);
                }
            }

            if (g_Globals.Misc.FastFire) {
                uint32_t fireAttr = Mem.ReadS<uint32_t>(localPlayer + Offsets::LocalPlayerAttributes);
                if (fireAttr) {
                    Mem.Write<float>(fireAttr + Offsets::FireIntervalScaleTwo, 0.3f);
                }
            }
            else {
                uint32_t fireAttr = Mem.ReadS<uint32_t>(localPlayer + Offsets::LocalPlayerAttributes);
                if (fireAttr) {
                    Mem.Write<float>(fireAttr + Offsets::FireIntervalScaleTwo, 1.0f); // default value
                }
            }

            if (g_Globals.Misc.AutoFire) {
                Data::autofire();
            }

            static uint32_t lastSpeedAddr = 0;
            static float lastSpeedPercentage = 0.0f;
            static std::unordered_map<uint32_t, float> originalRunSpeedValues;
            static std::unordered_map<uint32_t, float> originalWalkSpeedValues;
            static std::unordered_map<uint32_t, float> originalFallingSpeedValues;

            if (g_Globals.Misc.SpeedHack) {
                // Ensure both flags are in sync
                if (g_Globals.Misc.SpeedHack && !g_Globals.Misc.OffsetSpeed) {
                    g_Globals.Misc.OffsetSpeed = true;
                }

                uint32_t speedAddr = 0;
                if (Mem.Read(localPlayer + Offsets::PlayerAttributes, speedAddr) && speedAddr != 0) {
                    // If speed address changed (new game/match), clear old entry
                    if (lastSpeedAddr != 0 && lastSpeedAddr != speedAddr) {
                        // New game/match detected - clear old speed values from dictionaries
                        originalRunSpeedValues.erase(lastSpeedAddr);
                        originalWalkSpeedValues.erase(lastSpeedAddr);
                        originalFallingSpeedValues.erase(lastSpeedAddr);
                        lastSpeedPercentage = 0.0f; // Reset to force reapplication
                    }
                    lastSpeedAddr = speedAddr;

                    // Convert multiplier (1x-10x) to percentage equivalent for original formula
                    // 1x = 0%, 10x = 900% (to match original behavior)
                    float speedPercentage = (g_Globals.Misc.SpeedValue - 1.0f) * 100.0f;

                    // Apply RunSpeedUpScale
                    float currentRunSpeed = 1.0f;
                    if (Mem.Read(speedAddr + Offsets::RunSpeedUpScale, currentRunSpeed)) {
                        // Store original speed value if not already stored
                        if (originalRunSpeedValues.find(speedAddr) == originalRunSpeedValues.end()) {
                            originalRunSpeedValues[speedAddr] = currentRunSpeed;
                        }

                        // Calculate target speed using original percentage formula
                        float originalSpeed = originalRunSpeedValues[speedAddr];
                        float targetSpeed = originalSpeed + (speedPercentage / 100.0f) * 0.75f;

                        // Continuously apply speed - check if current speed matches target
                        // This ensures speed persists even if game resets it
                        if (std::abs(currentRunSpeed - targetSpeed) > 0.001f || std::abs(lastSpeedPercentage - speedPercentage) > 0.001f) {
                            Mem.Write<float>(speedAddr + Offsets::RunSpeedUpScale, targetSpeed);
                            lastSpeedPercentage = speedPercentage;
                        }
                    }

                    // Apply WalkDownSpeedScale

                }
            }
            else {
                // Speed is OFF - restore original speed values (default 1.0f)
                if (lastSpeedAddr != 0) {
                    uint32_t speedAddr = 0;
                    if (Mem.Read(localPlayer + Offsets::PlayerAttributes, speedAddr) && speedAddr != 0) {
                        // Restore RunSpeedUpScale
                        float currentRunSpeed = 1.0f;
                        if (Mem.Read(speedAddr + Offsets::RunSpeedUpScale, currentRunSpeed) && std::abs(currentRunSpeed - 1.0f) > 0.001f) {
                            auto it = originalRunSpeedValues.find(speedAddr);
                            if (it != originalRunSpeedValues.end()) {
                                Mem.Write<float>(speedAddr + Offsets::RunSpeedUpScale, it->second);
                            }
                            else {
                                Mem.Write<float>(speedAddr + Offsets::RunSpeedUpScale, 1.0f);
                            }
                        }


                    }
                }
                lastSpeedAddr = 0; // Reset tracking
            }

            ForceAimCpp::Tick(localPlayer);
            PullEnemy360Cpp::Tick(localPlayer);

            static float originalRightOffset = 0.0f;
            static bool cameraHackActive = false;
            static bool originalRightOffsetStored = false;
            static float lastCameraOffset = 0.0f;

            // Horizontal camera hack: shift camera left/right by modifying FollowCamera.m_RightOffset
            if (g_Globals.Misc.CameraHack) {
                uint32_t localPlayerAddr = g_Globals.EspConfig.LocalPlayer;
                if (localPlayerAddr != 0) {
                    uint32_t followCamera = 0;
                    if (Mem.Read(localPlayerAddr + Offsets::FollowCamera, followCamera) && followCamera != 0) {
                        // Store original m_RightOffset only once when first enabled
                        if (!originalRightOffsetStored) {
                            float currentRightOffset = 0.0f;
                            if (Mem.Read(followCamera + Offsets::FollowCamera_m_RightOffset, currentRightOffset)) {
                                // Remove old offset to get original value (internal runtime value)
                                if (cameraHackActive && std::abs(lastCameraOffset) > 0.01f) {
                                    // We previously added a scaled offset based on lastCameraOffset, so subtract it out
                                    float lastInternalOffset = (lastCameraOffset / 45.0f) * 2.0f; // must match scale used below
                                    originalRightOffset = currentRightOffset - lastInternalOffset;
                                }
                                else {
                                    originalRightOffset = currentRightOffset;
                                }
                                originalRightOffsetStored = true;
                                cameraHackActive = true;
                            }
                        }

                        // Apply camera offset (negative = left, positive = right)
                        // Map UI degrees (-45..+45) into internal offset range with increased radius (about -2.0..+2.0)
                        float internalOffset = (g_Globals.Misc.CameraOffset / 45.0f) * 2.0f;
                        float targetRightOffset = originalRightOffset + internalOffset;

                        // Write to m_RightOffset (internal runtime field) - always apply, even if offset is 0
                        Mem.Write<float>(followCamera + Offsets::FollowCamera_m_RightOffset, targetRightOffset);
                        lastCameraOffset = g_Globals.Misc.CameraOffset;
                    }
                }
            }
            else {
                // Camera horizontal disabled - restore original m_RightOffset
                if (cameraHackActive && originalRightOffsetStored) {
                    uint32_t localPlayerAddr = g_Globals.EspConfig.LocalPlayer;
                    if (localPlayerAddr != 0) {
                        uint32_t followCamera = 0;
                        if (Mem.Read(localPlayerAddr + Offsets::FollowCamera, followCamera) && followCamera != 0) {
                            Mem.Write<float>(followCamera + Offsets::FollowCamera_m_RightOffset, originalRightOffset);
                        }
                    }
                }
                cameraHackActive = false;
                originalRightOffsetStored = false;
                // Don't reset lastCameraOffset - keep it for next time
            }


            static float originalUpOffset = 0.0f;
            static bool cameraVerticalActive = false;
            static bool originalUpOffsetStored = false;
            static float lastCameraVerticalOffset = 0.0f;

            // Vertical camera hack: shift camera up/down by modifying FollowCamera.m_UpOffset
            if (g_Globals.Misc.CameraVertical) {
                uint32_t localPlayerAddr = g_Globals.EspConfig.LocalPlayer;
                if (localPlayerAddr != 0) {
                    uint32_t followCamera = 0;
                    if (Mem.Read(localPlayerAddr + Offsets::FollowCamera, followCamera) && followCamera != 0) {
                        // Store original m_UpOffset only once when first enabled
                        if (!originalUpOffsetStored) {
                            float currentUpOffset = 0.0f;
                            if (Mem.Read(followCamera + Offsets::FollowCamera_m_UpOffset, currentUpOffset)) {
                                if (cameraVerticalActive && std::abs(lastCameraVerticalOffset) > 0.01f) {
                                    float lastInternalOffset = (lastCameraVerticalOffset / 45.0f) * 2.0f;
                                    originalUpOffset = currentUpOffset - lastInternalOffset;
                                }
                                else {
                                    originalUpOffset = currentUpOffset;
                                }
                                originalUpOffsetStored = true;
                                cameraVerticalActive = true;
                            }
                        }

                        // Apply camera vertical offset (negative = down, positive = up)
                        // Map degrees to internal offset range with increased radius (about -2.0..+2.0)
                        float internalOffset = (g_Globals.Misc.CameraVerticalOffset / 45.0f) * 2.0f;
                        float targetUpOffset = originalUpOffset + internalOffset;

                        // Always apply, even if offset is 0
                        Mem.Write<float>(followCamera + Offsets::FollowCamera_m_UpOffset, targetUpOffset);
                        lastCameraVerticalOffset = g_Globals.Misc.CameraVerticalOffset;
                    }
                }
            }
            else {
                // Camera vertical disabled - restore original m_UpOffset
                if (cameraVerticalActive && originalUpOffsetStored) {
                    uint32_t localPlayerAddr = g_Globals.EspConfig.LocalPlayer;
                    if (localPlayerAddr != 0) {
                        uint32_t followCamera = 0;
                        if (Mem.Read(localPlayerAddr + Offsets::FollowCamera, followCamera) && followCamera != 0) {
                            Mem.Write<float>(followCamera + Offsets::FollowCamera_m_UpOffset, originalUpOffset);
                        }
                    }
                }
                cameraVerticalActive = false;
                originalUpOffsetStored = false;
                // Don't reset lastCameraVerticalOffset - keep it for next time
            }

            // Vision Hack — FollowCamera.FOVOffset (+0x48)
            {
                static bool visionHackActive = false;
                constexpr float kVisionDistance = 75.0f;
                constexpr float kVisionDefault = 0.0f;

                uint32_t followCamera = 0;
                if (Mem.Read(localPlayer + Offsets::FollowCamera, followCamera) && followCamera != 0) {
                    if (g_Globals.Misc.VisionHackEnabled) {
                        Mem.Write<float>(followCamera + Offsets::FollowCamera_m_VisionSpeed, kVisionDistance);
                        visionHackActive = true;
                    }
                    else if (visionHackActive) {
                        Mem.Write<float>(followCamera + Offsets::FollowCamera_m_VisionSpeed, kVisionDefault);
                        visionHackActive = false;
                    }
                }
            }

            if (g_Globals.Misc.RemoveFireDelay) {
                uint64_t currentWeapon = Mem.ReadS<uint64_t>(localPlayer + Offsets::Weapon);
                if (currentWeapon != 0) {
                    // Reset last firing time to remove fire delay
                    Mem.Write<float>(currentWeapon + Offsets::Weapon_LastFiringTime, 0.0f);

                    // Reset not ready time from pre weapon (removes first fire delay)
                    Mem.Write<float>(currentWeapon + Offsets::Weapon_NotReadyTimeFromPreWeapon, 0.0f);

                    // Reset fire tick counts to bypass fire delays
                    Mem.Write<uint32_t>(currentWeapon + Offsets::Weapon_LastFireTickCount, 0);
                    Mem.Write<uint32_t>(currentWeapon + Offsets::Weapon_LastStartFireReqTickCount, 0);

                    // Modify FireComponent fire interval if it exists
                    uint64_t fireComponent = Mem.ReadS<uint64_t>(currentWeapon + Offsets::Weapon_FireComponent);
                    if (fireComponent != 0) {
                        Mem.Write<float>(fireComponent + Offsets::WeaponFireComponent_FireInterval, 0.001f);
                    }

                    // Remove sniper pull bolt delay (works for all weapons but mainly affects snipers)
                    Mem.Write<float>(currentWeapon + Offsets::Weapon_LastPullBoltTime, 0.0f);
                }
            }



            // TeleKill (B!T) — pull closest enemy within range to ~1m in front of you
            if (g_Globals.Misc.TeleKill) {
                uint32_t localRootBonePtr = 0;
                if (Mem.Read(localPlayer + Offsets::Bones::Root, localRootBonePtr) && localRootBonePtr != 0) {
                    Vector3 localRootPos;
                    if (TransformUtils::GetNodePosition(localRootBonePtr, localRootPos)) {
                        constexpr float maxDist = 10.0f;
                        const float keepDist = (g_Globals.Misc.TeleKillKeepDistance > 0.1f)
                            ? g_Globals.Misc.TeleKillKeepDistance : 1.0f;

                        float closestDistance = FLT_MAX;
                        Player* bestEnemy = nullptr;

                        for (auto& pair : g_Globals.EspConfig.Entities) {
                            Player& ent = pair.second;
                            if (ent.Address == 0 || ent.Address == localPlayer)
                                continue;
                            if (!ent.IsKnown || ent.IsDead)
                                continue;
                            if (ent.IsTeam == Bool3::True)
                                continue;
                            if (g_Globals.AimBot.IgnoreKnocked && ent.IsKnocked)
                                continue;

                            uint32_t enemyRootBonePtr = 0;
                            if (!Mem.Read(ent.Address + Offsets::Bones::Root, enemyRootBonePtr) || enemyRootBonePtr == 0)
                                continue;

                            Vector3 enemyPos;
                            if (!TransformUtils::GetNodePosition(enemyRootBonePtr, enemyPos))
                                continue;

                            const float playerDistance = Vector3::Distance(localRootPos, enemyPos);
                            if (playerDistance > maxDist)
                                continue;

                            if (playerDistance < closestDistance) {
                                closestDistance = playerDistance;
                                bestEnemy = &ent;
                            }
                        }

                        if (bestEnemy != nullptr && bestEnemy->Address != localPlayer) {
                            uint32_t enemyRootBonePtr = 0;
                            if (Mem.Read(bestEnemy->Address + Offsets::Bones::Root, enemyRootBonePtr) && enemyRootBonePtr != 0) {
                                Vector3 enemyPos;
                                if (TransformUtils::GetNodePosition(enemyRootBonePtr, enemyPos)) {
                                    Vector3 dir = enemyPos - localRootPos;
                                    const float len = sqrtf(dir.X * dir.X + dir.Y * dir.Y + dir.Z * dir.Z);
                                    if (len > 0.0001f) {
                                        dir.X /= len;
                                        dir.Y /= len;
                                        dir.Z /= len;
                                    }
                                    else {
                                        dir = Vector3(0.0f, 0.0f, 1.0f);
                                    }

                                    const Vector3 targetPos = localRootPos + dir * keepDist;

                                    uint32_t enemyTransformValue = 0;
                                    if (Mem.Read(enemyRootBonePtr + 0x8, enemyTransformValue) && enemyTransformValue != 0) {
                                        uint32_t enemyTransformObjPtr = 0;
                                        if (Mem.Read(enemyTransformValue + 0x8, enemyTransformObjPtr) && enemyTransformObjPtr != 0) {
                                            uint32_t enemyMatrixValue = 0;
                                            if (Mem.Read(enemyTransformObjPtr + 0x20, enemyMatrixValue) && enemyMatrixValue != 0) {
                                                Mem.Write<Vector3>(enemyMatrixValue + 0x60, targetPos);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!runEntityScan) {
                if (g_Globals.General.DisableAllEffects && !g_Globals.EspConfig.Entities.empty())
                    FastRefreshEspPositions(mainPos);

                RageV2Aimbot();
                LegitAimbot();
                TriggerAimbot();
                SilentAimReworkedThread();
                Sniper::Aimbot();
                return;
            }

            // First pass: Load all entities from dictionary
            std::vector<uint32_t> currentEntities;
            for (uint32_t i = 0; i < entitiesCount; ++i) {
                uint32_t entity = Mem.ReadS<uint32_t>(entities + i * 0x10);
                if (entity == 0 || entity == localPlayer) continue;
                currentEntities.push_back(entity);
            }

            // Remove entities that are no longer in the game
            for (auto it = g_Globals.EspConfig.Entities.begin(); it != g_Globals.EspConfig.Entities.end();) {
                bool found = false;
                for (uint32_t currentEntity : currentEntities) {
                    if (it->first == currentEntity) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    it = g_Globals.EspConfig.Entities.erase(it);
                }
                else {
                    ++it;
                }
            }

            // Load/update all current entities with retry mechanism
            for (uint32_t entity : currentEntities) {
                try {
                    auto entry = g_Globals.EspConfig.Entities.find(entity);
                    Player* entityPtr = nullptr;

                    if (entry != g_Globals.EspConfig.Entities.end()) {
                        entityPtr = &entry->second;
                    }
                    else {
                        // New entity - create entry
                        entityPtr = &g_Globals.EspConfig.Entities[entity];
                        entityPtr->Address = entity; // Set address immediately
                        s_espEntityRetryCount[entity] = 0; // Initialize retry count
                    }

                    Vector3 previousHead = entityPtr ? entityPtr->Head : Vector3::Zero();

                    // Try to load entity data
                    bool loadSuccess = EntityData(entity, *entityPtr, mainPos);

                    if (!loadSuccess) {
                        // Entity failed to load - retry mechanism
                        int retryCount = s_espEntityRetryCount[entity];

                        // Retry up to 5 times before giving up
                        if (retryCount < 5) {
                            s_espEntityRetryCount[entity]++;
                            // Keep entity in map for retry, don't erase yet
                            // This ensures we keep trying to load it
                        }
                        else {
                            // Max retries reached - remove entity
                            g_Globals.EspConfig.Entities.erase(entity);
                            s_espEntityRetryCount.erase(entity);
                        }
                    }
                    else {
                        // Successfully loaded - reset retry count and update velocity
                        s_espEntityRetryCount[entity] = 0;
                        if (previousHead.X != 0 || previousHead.Y != 0 || previousHead.Z != 0) {
                            entityPtr->Velocity = (entityPtr->Head - previousHead) * 60.0f; // assume ~60Hz
                        }
                        entityPtr->LastHead = entityPtr->Head;
                    }
                }
                catch (const std::exception& e) {
                    // On exception, retry the entity
                    int retryCount = s_espEntityRetryCount[entity];
                    if (retryCount < 5) {
                        s_espEntityRetryCount[entity]++;
                    }
                    else {
                        g_Globals.EspConfig.Entities.erase(entity);
                        s_espEntityRetryCount.erase(entity);
                    }
                }
                catch (...) {
                    // Unknown exception - remove entity
                    g_Globals.EspConfig.Entities.erase(entity);
                    s_espEntityRetryCount.erase(entity);
                }
            }

            // Clear retry counts for entities that are no longer in the game
            for (auto it = s_espEntityRetryCount.begin(); it != s_espEntityRetryCount.end();) {
                bool found = false;
                for (uint32_t currentEntity : currentEntities) {
                    if (it->first == currentEntity) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    it = s_espEntityRetryCount.erase(it);
                }
                else {
                    ++it;
                }
            }

            {
                int liveEnemies = 0;
                for (const auto& pair : g_Globals.EspConfig.Entities) {
                    const Player& pl = pair.second;
                    if (pl.IsKnown && !pl.IsDead)
                        ++liveEnemies;
                }
                g_Globals.EspConfig.visibleEntityCount = liveEnemies;
            }

            RageV2Aimbot();
            LegitAimbot();
            TriggerAimbot();
            SilentAimReworkedThread();
            Sniper::Aimbot();
        }
        catch (const std::exception& e) {
            //Reset();
            std::cerr << "Exception caught: " << e.what() << std::endl;
        }
        catch (...) {
            // Reset();
            std::cerr << "Unknown exception caught!" << std::endl;
        }
    }


    bool Data::EntityData(uint32_t entity, Player& player, Vector3& mainPos)
    {
        try {
           

            uint32_t avatarManager = Mem.ReadS<uint32_t>(entity + Offsets::AvatarManager);
            if (avatarManager == 0) {
                return false; // Don't erase here - let retry mechanism handle it
            }

            uint32_t avatar = Mem.ReadS<uint32_t>(avatarManager + Offsets::Avatar);
            if (avatar == 0) {
                return false; // Don't erase here - let retry mechanism handle it
            }

            bool isVisible = Mem.ReadS<bool>(avatar + Offsets::Avatar_IsVisible);
            player.IsVisible = isVisible;
            // Visibility filtering is done in Visual.cpp (Wukong). Keep all enemies in the map for aim/sniper.

            uint32_t avatarData = Mem.ReadS<uint32_t>(avatar + Offsets::Avatar_Data);
            if (avatarData == 0) {
                return false; // Don't erase here - let retry mechanism handle it
            }

            bool isTeam = Mem.ReadS<bool>(avatarData + Offsets::Avatar_Data_IsTeam);
            player.IsTeam = isTeam ? Bool3::True : Bool3::False;
            player.IsKnown = !isTeam;

            if (player.IsTeam == Bool3::True || !player.IsKnown) {
                return false;
            }

            uint32_t shadowBase = Mem.ReadS<uint32_t>(entity + Offsets::Player_ShadowBase);
            if (shadowBase != 0) {
                int xpose = Mem.ReadS<int>(shadowBase + Offsets::XPose);
                player.IsKnocked = (xpose == 8);
            }

            player.Gun = ReadEquippedWeaponId(entity);

            player.IsDead = Mem.ReadS<bool>(entity + Offsets::Player_IsDead);

            player.IsBot = Mem.ReadS<bool>(entity + Offsets::IsClientBot);

            player.Address = entity;

          

          

            uint32_t dataPool = Mem.ReadS<uint32_t>(entity + Offsets::Player_Data);
            if (dataPool != 0) {
                uint32_t poolObj = Mem.ReadS<uint32_t>(dataPool + 0x8);
                if (poolObj != 0) {
                    uint32_t pool = Mem.ReadS<uint32_t>(poolObj + 0x10);
                    if (pool != 0) {
                        player.Health = Mem.ReadS<short>(pool + 0x10);
                    }
                }
            }

            std::map<uint32_t, Vector3*> boneMap = {
                 { (uint32_t)Offsets::Bones::Head, &player.Head },
                 { (uint32_t)Offsets::Bones::Neck, &player.Neck },
                 { (uint32_t)Offsets::Bones::LeftShoulder, &player.LeftShoulder },
                 { (uint32_t)Offsets::Bones::RightShoulder, &player.RightShoulder },
                 { (uint32_t)Offsets::Bones::LeftElbow, &player.LeftElbow },
                 { (uint32_t)Offsets::Bones::RightElbow, &player.RightElbow },
                 { (uint32_t)Offsets::Bones::LeftWrist, &player.LeftWrist },
                 { (uint32_t)Offsets::Bones::RightWrist, &player.RightWrist },
                 { (uint32_t)Offsets::Bones::LeftHand, &player.LeftHand },
                 { (uint32_t)Offsets::Bones::RightHand, &player.RightHand },
                 { (uint32_t)Offsets::Bones::Hip, &player.Hip },
                 { (uint32_t)Offsets::Bones::Groin, &player.Groin },
                 { (uint32_t)Offsets::Bones::Root, &player.Root },
                 { (uint32_t)Offsets::Bones::RootBone, &player.RootBone },
                 { (uint32_t)Offsets::Bones::LeftAnkle, &player.LeftAnkle },
                 { (uint32_t)Offsets::Bones::RightAnkle, &player.RightAnkle },
                 { (uint32_t)Offsets::Bones::LeftFoot, &player.LeftFoot },
                 { (uint32_t)Offsets::Bones::RightFoot, &player.RightFoot }
            };

            std::vector<uint32_t> bonePointers(boneMap.size(), 0);

            int index = 0;
            for (const auto& [offset, boneVector] : boneMap) {
                Mem.Read<uint32_t>(entity + offset, bonePointers[index]);
                index++;
            }

            index = 0;
            for (const auto& [offset, boneVector] : boneMap) {
                if (bonePointers[index] != 0) {
                    if (!TransformUtils::GetNodePosition(bonePointers[index], *boneVector))
                        *boneVector = Vector3::Zero();
                }
                else {
                    *boneVector = Vector3::Zero();
                }
                index++;
            }

            if (player.Head != Vector3::Zero()) {
                player.Distance = Vector3::Distance(mainPos, player.Head);
            }

            if (player.Head == Vector3::Zero()) {
                return false;
            }

            {
                const std::string name = Mem.ReadUnityPlayerNameFromEntity(entity, Offsets::Player_Name);
                if (!name.empty())
                    player.Name = name;
            }

            player.RankText.clear();
            if (player.IsBot)
                player.RankText = "BOT";
            else
                RankEsp::ReadRankFromEntity(entity, player.RankText);

            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception caught: " << std::string(e.what()) << std::endl;
            g_Globals.EspConfig.Entities.erase(entity);
            return false;
        }
    }

    void Data::Reset() {
        try {
            ResetEspCache();

            if (g_Globals.EspConfig.Matrix) {
                g_Globals.EspConfig.Matrix = false;
                g_Globals.EspConfig.ViewMatrix = Matrix4x4();
            }

            s_espEntityRetryCount.clear();
            s_espWorkLastLocalPlayer = 0;
            g_Globals.EspConfig.previousCount = 0;
            g_Globals.EspConfig.MainCamera = Vector3(0, 0, 0);
            g_Globals.Loot.GroundLoot.clear();

            g_TimerManager.StopForReset(g_Globals.Visuals.InvalidTimer);
            RestoreSpeedTimer();
        }
        catch (const std::exception& e) {
            std::cerr << "Error clearing game state: " << e.what() << std::endl;
        }
    }

    void Data::DrawInvalidMatchTimer(float screenWidth, float screenHeight)
    {
        g_TimerManager.DrawTimer(screenWidth, screenHeight, g_Globals.Visuals.InvalidTimer);
    }
}

namespace SpinPlayer
{
    static std::atomic<bool> isRunning{ false };
    static std::thread workThread;
    static float currentYaw = 0.0f;

    // UI stores 1xâ€“15x; cubic tail makes 15x much faster than linear scaling.
    static float DegreesPerTickFromMultiplier(float mult)
    {
        mult = std::clamp(mult, 1.0f, 15.0f);
        const float m2 = mult * mult;
        const float m3 = m2 * mult;
        return 5.0f * mult + 0.55f * m2 + 0.07f * m3;
    }

    static int TickSleepMsFromMultiplier(float mult)
    {
        mult = std::clamp(mult, 1.0f, 15.0f);
        if (mult >= 12.0f) return 4;
        if (mult >= 8.0f) return 6;
        if (mult >= 4.0f) return 8;
        return 10;
    }

    static void Loop()
    {
        constexpr float kPi = 3.14159265f;

        while (isRunning.load())
        {
            try
            {
                uint32_t localPlayer = g_Globals.EspConfig.LocalPlayer;
                if (localPlayer == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }

                bool isDead = false;
                if (!Mem.Read(localPlayer + Offsets::Player_IsDead, isDead) || isDead)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }

                uint32_t localRoot = 0;
                if (!Mem.Read(localPlayer + static_cast<uint32_t>(Offsets::Bones::Root), localRoot) || localRoot == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                uint32_t t1 = 0;
                if (!Mem.Read(localRoot + 0x8, t1) || t1 == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                uint32_t t2 = 0;
                if (!Mem.Read(t1 + 0x8, t2) || t2 == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                uint32_t localMatrix = 0;
                if (!Mem.Read(t2 + 0x20, localMatrix) || localMatrix == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                Vector3 currentPos{};
                if (!Mem.Read(localMatrix + 0x60, currentPos))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                const float mult = g_Globals.Misc.SpinPlayerSpeed;
                const float speedRad = DegreesPerTickFromMultiplier(mult) * (kPi / 180.0f);
                currentYaw += speedRad;
                if (currentYaw > kPi * 2.0f)
                    currentYaw -= kPi * 2.0f;

                const float halfYaw = currentYaw * 0.5f;
                const Quaternion spinRotation(0.0f, sinf(halfYaw), 0.0f, cosf(halfYaw));

                Mem.Write(localMatrix + 0x60, currentPos);
                Mem.Write(localMatrix + 0x70, spinRotation);

                std::this_thread::sleep_for(std::chrono::milliseconds(TickSleepMsFromMultiplier(mult)));
            }
            catch (...)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
    }

    void Start()
    {
        if (isRunning.load()) return;
        isRunning = true;
        workThread = std::thread(Loop);
        workThread.detach();
    }

    void Stop()
    {
        isRunning = false;
        currentYaw = 0.0f;
    }
}

namespace FlyHack_LocalPlayer
{
    // OFFSETS
    constexpr uint32_t MOVEMENT_COMPONENT_OFFSET = 0x1178;
    constexpr uint32_t POSITION_OFFSET = 0x1C;
    constexpr uint32_t VSPEED_OFFSET = 0x28;
    constexpr uint32_t IS_GROUNDED_OFFSET = 0x150;

    // SPEED CONFIG
    constexpr float HORIZONTAL_SPEED = 6.0f;
    constexpr float UP_SPEED = 3.0f;
    constexpr float DOWN_SPEED = -3.0f;
    constexpr float HOVER_SPEED = 0.02f;

    // HEIGHT LIMIT
    constexpr float MAX_FLY_HEIGHT = 9000.0f;
    static float baseHeight = 0.0f;
    static bool heightInitialized = false;

    static std::atomic<bool> isRunning{ false };
    static std::thread workThread;

    void ApplyFly()
    {
        uint32_t player = g_Globals.EspConfig.LocalPlayer;
        if (player == 0) return;

        uint32_t moveComp = 0;
        if (!Mem.Read(player + MOVEMENT_COMPONENT_OFFSET, moveComp) || moveComp == 0)
            return;

        // keep airborne
        Mem.Write<uint8_t>(moveComp + IS_GROUNDED_OFFSET, 0);

        // read position
        Vector3 pos;
        if (!Mem.Read<Vector3>(moveComp + POSITION_OFFSET, pos))
            return;

        // init base height
        if (!heightInitialized)
        {
            baseHeight = pos.Y;
            heightInitialized = true;
        }

        float maxAllowedHeight = baseHeight + MAX_FLY_HEIGHT;

        // camera direction
        Vector3 forward(
            g_Globals.EspConfig.ViewMatrix.m02,
            g_Globals.EspConfig.ViewMatrix.m12,
            g_Globals.EspConfig.ViewMatrix.m22
        );
        Vector3 right(
            g_Globals.EspConfig.ViewMatrix.m00,
            g_Globals.EspConfig.ViewMatrix.m10,
            g_Globals.EspConfig.ViewMatrix.m20
        );

        Vector3 fwd = Vector3::Normalized(Vector3(forward.X, 0.0f, forward.Z));
        Vector3 rgt = Vector3::Normalized(Vector3(right.X, 0.0f, right.Z));

        Vector3 moveDir = Vector3::Zero();

        if (GetAsyncKeyState(0x57) & 0x8000) moveDir = moveDir + fwd; // W
        if (GetAsyncKeyState(0x53) & 0x8000) moveDir = moveDir - fwd; // S
        if (GetAsyncKeyState(0x44) & 0x8000) moveDir = moveDir + rgt; // D
        if (GetAsyncKeyState(0x41) & 0x8000) moveDir = moveDir - rgt; // A

        float moveLen = std::sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
        if (moveLen > 0.0f) {
            moveDir.X = (moveDir.X / moveLen) * HORIZONTAL_SPEED;
            moveDir.Z = (moveDir.Z / moveLen) * HORIZONTAL_SPEED;
        }

        pos.X += moveDir.X * 0.016f;
        pos.Z += moveDir.Z * 0.016f;

        // height limit
        if (pos.Y > maxAllowedHeight)
            pos.Y = maxAllowedHeight;

        Mem.Write<Vector3>(moveComp + POSITION_OFFSET, pos);

        float vSpeed = HOVER_SPEED;

        if ((GetAsyncKeyState(0x20) & 0x8000) && pos.Y < maxAllowedHeight - 0.1f) // SPACE
            vSpeed = UP_SPEED;
        else if (GetAsyncKeyState(0xA2) & 0x8000) // LCONTROL
            vSpeed = DOWN_SPEED;

        Mem.Write<float>(moveComp + VSPEED_OFFSET, vSpeed);
    }

    static void Loop()
    {
        while (isRunning.load())
        {
            try
            {
                // Only run fly hack when local player exists and feature is enabled
                if (g_Globals.EspConfig.LocalPlayer != 0 && g_Globals.Misc.FlyHackInternalEnabled)
                {
                    ApplyFly();
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                    continue;
                }
            }
            catch (...) {}

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void Start()
    {
        if (isRunning.load()) return;
        isRunning = true;
        heightInitialized = false;

        workThread = std::thread(Loop);
        workThread.detach();
    }

    void Stop()
    {
        isRunning = false;
    }
}

namespace NoGravityFly
{
    // Offsets from updated dump (porulfly / Player.IFGAOAHPNOC) — Silent Aim X
    constexpr uint32_t MOVEMENT_COMPONENT_OFFSET = 0x139C; // Player.IFGAOAHPNOC
    constexpr uint32_t POSITION_OFFSET = 0x20;              // POAJHDKFLGE
    constexpr uint32_t VSPEED_OFFSET = 0x2C;                // EPCIJJLKNCO
    constexpr uint32_t IS_GROUNDED_OFFSET = 0x13F0;         // Player.m_GroundHitResult

    constexpr float HORIZONTAL_SPEED = 6.0f;
    constexpr float UP_SPEED = 3.0f;
    constexpr float DOWN_SPEED = -3.0f;
    constexpr float HOVER_SPEED = 0.02f;
    constexpr float MAX_FLY_HEIGHT = 9000.0f;

    static float baseHeight = 0.0f;
    static bool heightInitialized = false;
    static bool boostApplied = false;
    static std::atomic<bool> isRunning{ false };
    static std::thread workThread;

    static void ApplyFly()
    {
        const uint32_t player = g_Globals.EspConfig.LocalPlayer;
        if (player == 0) return;

        uint32_t moveComp = 0;
        if (!Mem.Read(player + MOVEMENT_COMPONENT_OFFSET, moveComp) || moveComp == 0)
            return;

        // keep airborne (ground flag is on Player, not PhysXData)
        Mem.Write<uint8_t>(player + IS_GROUNDED_OFFSET, 0);

        Vector3 pos{};
        if (!Mem.Read<Vector3>(moveComp + POSITION_OFFSET, pos))
            return;

        if (!heightInitialized)
        {
            baseHeight = pos.Y;
            heightInitialized = true;
        }

        // initial boost: launch upward on first enable
        if (!boostApplied)
        {
            pos.Y += 20.0f;
            boostApplied = true;
            Mem.Write<Vector3>(moveComp + POSITION_OFFSET, pos);
            Mem.Write<float>(moveComp + VSPEED_OFFSET, UP_SPEED);
            return;
        }

        const float maxAllowedHeight = baseHeight + MAX_FLY_HEIGHT;

        if (!g_Globals.EspConfig.Matrix)
            return;

        const Matrix4x4& matrix = g_Globals.EspConfig.ViewMatrix;
        Vector3 forward(matrix.m02, matrix.m12, matrix.m22);
        Vector3 right(matrix.m00, matrix.m10, matrix.m20);

        Vector3 fwd = Vector3::Normalized(Vector3(forward.X, 0.0f, forward.Z));
        Vector3 rgt = Vector3::Normalized(Vector3(right.X, 0.0f, right.Z));

        Vector3 moveDir = Vector3::Zero();

        if (GetAsyncKeyState(0x57) & 0x8000) moveDir = moveDir + fwd; // W
        if (GetAsyncKeyState(0x53) & 0x8000) moveDir = moveDir - fwd; // S
        if (GetAsyncKeyState(0x44) & 0x8000) moveDir = moveDir + rgt; // D
        if (GetAsyncKeyState(0x41) & 0x8000) moveDir = moveDir - rgt; // A

        const float moveLen = std::sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
        if (moveLen > 0.0f)
        {
            moveDir.X = (moveDir.X / moveLen) * HORIZONTAL_SPEED;
            moveDir.Z = (moveDir.Z / moveLen) * HORIZONTAL_SPEED;
        }

        pos.X += moveDir.X * 0.016f;
        pos.Z += moveDir.Z * 0.016f;

        if (pos.Y > maxAllowedHeight)
            pos.Y = maxAllowedHeight;

        Mem.Write<Vector3>(moveComp + POSITION_OFFSET, pos);

        float vSpeed = HOVER_SPEED;
        if ((GetAsyncKeyState(0x20) & 0x8000) && pos.Y < maxAllowedHeight - 0.1f) // SPACE
            vSpeed = UP_SPEED;
        else if (GetAsyncKeyState(0xA2) & 0x8000) // LCONTROL
            vSpeed = DOWN_SPEED;

        Mem.Write<float>(moveComp + VSPEED_OFFSET, vSpeed);
    }

    static void Loop()
    {
        while (isRunning.load())
        {
            try
            {
                if (!g_Globals.Misc.NoGravityFlyEnabled || g_Globals.EspConfig.LocalPlayer == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(40));
                    continue;
                }

                ApplyFly();
            }
            catch (...) {}

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void Start()
    {
        if (isRunning.load()) return;
        isRunning = true;
        heightInitialized = false;
        boostApplied = false;
        workThread = std::thread(Loop);
        workThread.detach();
    }

    void Stop()
    {
        isRunning = false;
        heightInitialized = false;
        boostApplied = false;
    }
}

