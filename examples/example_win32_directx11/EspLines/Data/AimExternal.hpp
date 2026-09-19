#pragma once
#include <cstdint>
#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>
#include <examples/example_win32_directx11/EspLines/Offsets.hpp>
#include <examples/example_win32_directx11/EspLines/Math/Vector/Vector3.hpp>
#include <chrono>

namespace AimRage {
    class AimbotMode {
    public:
        struct PersistentEntityCache
        {
            uintptr_t hipAddr;
            uint32_t originalHip;
            Vector3 lastPosition;
            std::chrono::high_resolution_clock::time_point lastUpdate;
        };
        /*struct AimCache
        {

            uintptr_t hipAddr;
            uint32_t originalHip;
            bool modified;

            std::chrono::high_resolution_clock::time_point lastWriteTime;
        };*/

        //static std::unordered_map<uintptr_t, AimCache> entityCache;


        static void ExternalAimbot();
        static void StartAimbot();
        static void StopAimbot();

    private:
        static bool EntityData(uint32_t entity, Player& player, Vector3& mainPos);
        static Vector3 GetHitBoxPosition(const Player& entity);
        static Player* FindClosestEnemy();
    };
}
