#pragma once

#include <cstdint>
#include <string>
#include <examples/example_win32_directx11/EspLines/Math/Vector/Vector3.hpp>

struct GroundLootEntry {
    uint32_t ItemId = 0;
    Vector3 Position{};
    std::string Name;
    float Distance = 0.f;
};
