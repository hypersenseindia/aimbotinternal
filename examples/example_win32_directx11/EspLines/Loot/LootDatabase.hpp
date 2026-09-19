#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <imgui.h>

struct LootCatalogItem {
    uint32_t id = 0;
    std::string name;
    int category = 0;
};

namespace LootDatabase {
    int CategoryCount();
    const char* CategoryName(int index);
    const char* const* CategoryNameList();

    const std::unordered_map<uint32_t, std::string>& ItemNames();
    const std::vector<std::vector<uint32_t>>& Categories();
    const std::vector<LootCatalogItem>& Catalog();

    // Maps pickup/raw ids (21004, 20004, 0x00020004) to catalog ids (e.g. 4 = AWM).
    uint32_t NormalizeItemId(uint32_t rawItemId);

    std::string GetName(uint32_t itemId);
    int GetCategoryIndex(uint32_t itemId);
    ImVec4 GetColor(uint32_t itemId);
}
