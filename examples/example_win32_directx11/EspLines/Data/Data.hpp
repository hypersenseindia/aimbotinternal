#pragma once
#include <cstdint>
#include <examples/example_win32_directx11/EspLines/Math/Vector/Vector3.hpp>
#include <examples/example_win32_directx11/EspLines/Player.h>
#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>

namespace FWork {
    namespace PullEnemy360Cpp {
        void Stop();
    }
    class Data {
    public:
        static void SilentAimReworkedThread();
        static void autofire();
        static void Work();
        /// Mirror AotForms ResetCache / NoCache: clear phys cache + entity map.
        static void ResetEspCache();
        /// Invalid match timer text; call only from the ESP render path (same ImGui frame as other ESP draws).
        static void DrawInvalidMatchTimer(float screenWidth, float screenHeight);

    private:
        static bool EntityData(uint32_t entity, Player& player, Vector3& mainPos);
        static void Reset();
    };
}
