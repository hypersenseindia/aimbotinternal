#define NOMINMAX
#include <imgui.h>
#include <imgui_internal.h>

#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>
#include <examples/example_win32_directx11/EspLines/Offsets.hpp>


#include <random>
#include "examples/example_win32_directx11/EspLines/Data/AimExternal.hpp"
#include <examples/example_win32_directx11/EspLines/Math/WordToScreen.hpp>



std::atomic<bool> aimbotRunning2{ false };

std::thread aimbotThread2;

static bool lastKeyState = false;

static Player* lastTarget2 = nullptr;


static Player* FindBestTargetByFov(float maxFOV, float maxDistance, bool ignoreKnocked)
{
    float closestCross = FLT_MAX;

    Player* closestEntity = nullptr;

    Vector2 screenCenter(g_Globals.EspConfig.Width / 2.0f, g_Globals.EspConfig.Height / 2.0f);

    for (auto& pair : g_Globals.EspConfig.Entities)
    {
        Player* entity = &pair.second;

        if (!entity || entity->Address == 0 || entity->IsDead) continue;

        if (!entity->IsKnown) continue;

        if (ignoreKnocked && entity->Pose == XPose::Knocked) continue;

        float distance = Vector3::Distance(g_Globals.EspConfig.MainCamera, entity->Head);

        if (distance <= 0 || distance > maxDistance) continue;

        ImVec2 pos2D = W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, entity->Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);

        if (pos2D.x <= 0 || pos2D.y <= 0 || pos2D.x > g_Globals.EspConfig.Width || pos2D.y > g_Globals.EspConfig.Height)
            continue;

        float crossDist = std::hypot(pos2D.x - screenCenter.X, pos2D.y - screenCenter.Y);
        if (crossDist > maxFOV) continue;

        if (crossDist < closestCross)
        {
            closestCross = crossDist;
            closestEntity = entity;
        }
    }

    return closestEntity;
}




void AimRage::AimbotMode::ExternalAimbot()
{
    // ========== PERSISTENT STORAGE ==========
    static std::unordered_map<uintptr_t, PersistentEntityCache> entityDatabase;
    static std::unordered_map<uintptr_t, bool> entityDeadStatus;
    static std::unordered_map<uintptr_t, bool> entityModifiedStatus;

    static uintptr_t currentTarget = 0;
    static std::mt19937 gen(std::random_device{}());
    static bool wasKeyPressed = false;
    static std::chrono::steady_clock::time_point lastWriteTime;

    auto& AimBot = g_Globals.AimBot;
    auto& Esp = g_Globals.EspConfig;

    // ========== CLEANUP FUNCTION ==========
    auto CleanupAllEntities = [&]()
        {
            for (auto& [addr, cache] : entityDatabase)
            {
                if (entityModifiedStatus[addr])
                {
                    Mem.Write<uint32_t>(cache.hipAddr, cache.originalHip);
                }
            }

            entityDatabase.clear();
            entityDeadStatus.clear();
            entityModifiedStatus.clear();
            currentTarget = 0;
        };

    // ========== RESTORE SINGLE ENTITY ==========
    auto RestoreEntity = [&](uintptr_t addr)
        {
            auto it = entityDatabase.find(addr);
            if (it != entityDatabase.end() && entityModifiedStatus[addr])
            {
                Mem.Write<uint32_t>(it->second.hipAddr, it->second.originalHip);
                entityModifiedStatus[addr] = false;
            }
        };

    // ========== RESTORE DEAD ENTITIES ONLY ==========
    auto RestoreDeadEntities = [&]()
        {
            for (auto& [addr, isDead] : entityDeadStatus)
            {
                if (isDead && entityModifiedStatus[addr])
                {
                    auto it = entityDatabase.find(addr);
                    if (it != entityDatabase.end())
                    {
                        Mem.Write<uint32_t>(it->second.hipAddr, it->second.originalHip);
                        entityModifiedStatus[addr] = false;
                    }
                }
            }
        };

    // ========== UPDATE ENTITY DATABASE ==========
    auto UpdateEntityDatabase = [&]()
        {
            for (auto& pair : Esp.Entities)
            {
                Player* entity = &pair.second;
                if (!entity || !entity->Address)
                    continue;

                uintptr_t addr = entity->Address;

                if (entityDatabase.find(addr) == entityDatabase.end())
                {
                    uintptr_t hipAddr = addr + Offsets::Bones::Hip;
                    uint32_t originalHip = Mem.ReadS<uint32_t>(hipAddr);

                    entityDatabase[addr] = {
                        hipAddr,
                        originalHip,
                        entity->Head,
                        std::chrono::high_resolution_clock::now()
                    };
                    entityDeadStatus[addr] = entity->IsDead;
                    entityModifiedStatus[addr] = false;
                }
                else
                {
                    entityDatabase[addr].lastPosition = entity->Head;

                    if (entityDeadStatus[addr] && !entity->IsDead)
                    {
                        entityDeadStatus[addr] = false;
                        RestoreEntity(addr);
                    }
                    else if (!entityDeadStatus[addr] && entity->IsDead)
                    {
                        entityDeadStatus[addr] = true;
                        RestoreEntity(addr);
                    }
                }
            }

            std::vector<uintptr_t> toRemove;
            for (auto& [addr, cache] : entityDatabase)
            {
                bool stillExists = false;
                for (auto& pair : Esp.Entities)
                {
                    if (pair.second.Address == addr)
                    {
                        stillExists = true;
                        break;
                    }
                }
                if (!stillExists)
                {
                    RestoreEntity(addr);
                    toRemove.push_back(addr);
                }
            }

            for (uintptr_t addr : toRemove)
            {
                entityDatabase.erase(addr);
                entityDeadStatus.erase(addr);
                entityModifiedStatus.erase(addr);
            }
        };

    // ========== MAIN LOGIC ==========

    if (!AimBot.ExternalEnabled)
    {
        if (!entityDatabase.empty())
            CleanupAllEntities();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return;
    }

    // Continuous database update
    UpdateEntityDatabase();
    RestoreDeadEntities();

    // ========== INPUT CHECKS ==========
    bool keyDown = false;
    if (AimBot.ExternalBind == 0 || AimBot.ExternalBind == VK_LBUTTON)
        keyDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
    else
        keyDown = (GetAsyncKeyState(AimBot.ExternalBind) & 0x8000);

    bool keyJustReleased = (!keyDown && wasKeyPressed);
    wasKeyPressed = keyDown;

    CURSORINFO ci{};
    ci.cbSize = sizeof(CURSORINFO);
    bool cursorVisible = true;
    if (GetCursorInfo(&ci))
        cursorVisible = (ci.flags & CURSOR_SHOWING) != 0;

    bool shouldAim = (keyDown && !cursorVisible);

    // Handle key release - immediate restore
    if (keyJustReleased && currentTarget)
    {
        RestoreEntity(currentTarget);
        currentTarget = 0;
    }

    if (!shouldAim)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWriteTime).count();

        if (elapsed > 10 && currentTarget)
        {
            RestoreEntity(currentTarget);
            currentTarget = 0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return;
    }

    if (Esp.Width <= 0 || Esp.Height <= 0 || !Esp.Matrix)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return;
    }

    // ========== FIND BEST TARGET ==========
    Vector2 screenCenter(Esp.Width * 0.5f, Esp.Height * 0.5f);
    float bestCrosshairDist = FLT_MAX;
    Player* bestEntity = nullptr;
    uintptr_t bestEntityAddr = 0;

    for (auto& pair : Esp.Entities)
    {
        Player* entity = &pair.second;
        if (!entity || !entity->Address)
            continue;

        uintptr_t addr = entity->Address;

        if (entity->IsTeam == Bool3::True)
            continue;
        if (entityDeadStatus[addr])
            continue;
        if (AimBot.ExternalIgnoreKnocked && entity->Pose == XPose::Knocked)
            continue;
        if (AimBot.IgnoreTrainingBots && entity->Name.empty() && entity->IsBot)
            continue;

        Vector3 bonePos;
        switch (AimBot.ExternalBone)
        {
        case 0: bonePos = entity->Head; break;
        case 1: bonePos = entity->Neck; break;
        case 2: bonePos = entity->Hip; break;
        case 3: bonePos = entity->RightShoulder; break;
        case 4: bonePos = entity->LeftShoulder; break;
        case 5: bonePos = entity->RightElbow; break;
        case 6: bonePos = entity->LeftElbow; break;
        default: bonePos = entity->Head; break;
        }

        float worldDist = Vector3::Distance(Esp.MainCamera, bonePos);
        if (worldDist > AimBot.ExternalDistance)
            continue;

        ImVec2 screenPos = W2S::WorldToScreenImVec2(
            Esp.ViewMatrix,
            bonePos,
            Esp.Width,
            Esp.Height
        );

        if (screenPos.x <= 0 || screenPos.y <= 0 || screenPos.x > Esp.Width || screenPos.y > Esp.Height)
            continue;

        float crossDist = std::hypot(screenPos.x - screenCenter.X, screenPos.y - screenCenter.Y);

        if (crossDist > AimBot.ExternalFov)
            continue;

        if (crossDist < bestCrosshairDist)
        {
            bestCrosshairDist = crossDist;
            bestEntity = entity;
            bestEntityAddr = addr;
        }
    }

    if (!bestEntity)
    {
        if (currentTarget)
        {
            RestoreEntity(currentTarget);
            currentTarget = 0;
        }
        return;
    }

    // Target switching
    if (currentTarget && currentTarget != bestEntityAddr)
    {
        RestoreEntity(currentTarget);
        currentTarget = 0;
    }

    // Cache new entity
    if (entityDatabase.find(bestEntityAddr) == entityDatabase.end())
    {
        uintptr_t hipAddr = bestEntityAddr + Offsets::Bones::Hip;
        uint32_t originalHip = Mem.ReadS<uint32_t>(hipAddr);

        entityDatabase[bestEntityAddr] = {
            hipAddr,
            originalHip,
            bestEntity->Head,
            std::chrono::high_resolution_clock::now()
        };
        entityDeadStatus[bestEntityAddr] = bestEntity->IsDead;
        entityModifiedStatus[bestEntityAddr] = false;
    }

    // ========== BONE OFFSET ==========
    uintptr_t boneOffset;
    switch (AimBot.ExternalBone)
    {
    case 0: boneOffset = Offsets::Bones::Head; break;
    case 1: boneOffset = Offsets::Bones::Neck; break;
    case 2: boneOffset = Offsets::Bones::Hip; break;
    case 3: boneOffset = Offsets::Bones::RightShoulder; break;
    case 4: boneOffset = Offsets::Bones::LeftShoulder; break;
    case 5: boneOffset = Offsets::Bones::RightElbow; break;
    case 6: boneOffset = Offsets::Bones::LeftElbow; break;
    default: boneOffset = Offsets::Bones::Head; break;
    }

    // ========== APPLY AIMBOT ==========
    if (!entityDeadStatus[bestEntityAddr])
    {
        uint32_t boneCollider = Mem.ReadS<uint32_t>(bestEntityAddr + boneOffset);
        if (boneCollider)
        {
            auto& cache = entityDatabase[bestEntityAddr];
            Mem.Write<uint32_t>(cache.hipAddr, boneCollider);
            entityModifiedStatus[bestEntityAddr] = true;
            lastWriteTime = std::chrono::steady_clock::now();
            currentTarget = bestEntityAddr;
        }
    }

    std::uniform_int_distribution<int> delay(3, 8);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay(gen)));
}



void AimRage::AimbotMode::StartAimbot()
{
    if (aimbotRunning2) return;
    aimbotRunning2 = true;

    aimbotThread2 = std::thread([]()
        {
            while (aimbotRunning2)
            {
                bool didWork = false;




                if (g_Globals.AimBot.ExternalEnabled)
                {
                    AimRage::AimbotMode::ExternalAimbot();
                    didWork = true;
                }



                //-------------------------------------------------
                // CPU CONTROL
                //-------------------------------------------------
                if (!didWork)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });

    aimbotThread2.detach();
}

void AimRage::AimbotMode::StopAimbot()
{
    aimbotRunning2 = false;
    if (aimbotThread2.joinable())
    {
        aimbotThread2.join();
    }
}
