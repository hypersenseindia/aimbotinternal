#define IMGUI_DEFINE_MATH_OPERATORS
#include "LootVisual.hpp"

#include <imgui.h>
#include <cstdio>
#include <string>

#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Math/WordToScreen.hpp>
#include <examples/example_win32_directx11/EspLines/Loot/LootDatabase.hpp>

namespace {

    static bool IsOnScreen(const Vector2& p, int screenW, int screenH, float margin = 80.f) {
        return p.X >= -margin && p.Y >= -margin && p.X <= screenW + margin && p.Y <= screenH + margin;
    }

    static void DrawOutlinedText(ImDrawList* draw, const ImVec2& pos, ImU32 col, const char* text) {
        const ImU32 shadow = IM_COL32(0, 0, 0, 220);
        for (float ox = -1.f; ox <= 1.f; ox += 1.f) {
            for (float oy = -1.f; oy <= 1.f; oy += 1.f) {
                if (ox == 0.f && oy == 0.f)
                    continue;
                draw->AddText(ImVec2(pos.x + ox, pos.y + oy), shadow, text);
            }
        }
        draw->AddText(pos, col, text);
    }

} // namespace

namespace ESP {

void Loot() {
    if (!g_Globals.Loot.Enabled || !g_Globals.EspConfig.Matrix)
        return;

    ImDrawList* const draw = ImGui::GetBackgroundDrawList();
    if (!draw)
        return;

    const int screenW = g_Globals.EspConfig.Width;
    const int screenH = g_Globals.EspConfig.Height;
    const Matrix4x4& view = g_Globals.EspConfig.ViewMatrix;

    // Only draw loot for items explicitly enabled in the picker (checkbox on).
    if (!g_Globals.Loot.HasActiveFilter())
        return;

    const std::vector<GroundLootEntry> lootSnapshot = g_Globals.Loot.GroundLoot;

    for (const GroundLootEntry& loot : lootSnapshot) {
        if (!loot.ItemId)
            continue;
        if (!g_Globals.Loot.IsItemFiltered(LootDatabase::NormalizeItemId(loot.ItemId)))
            continue;

        Vector2 screen = W2S::WorldToScreen(view, loot.Position, screenW, screenH);
        if (!IsOnScreen(screen, screenW, screenH, 40.f))
            continue;

        const ImVec4 colV = LootDatabase::GetColor(loot.ItemId);
        const ImU32 col = ImGui::GetColorU32(colV);

        std::string label = loot.Name;
        if (g_Globals.Loot.ShowDistanceOnLabel) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s [%dm]", loot.Name.c_str(), static_cast<int>(loot.Distance));
            label = buf;
        }

        const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
        const ImVec2 textPos(screen.X - textSize.x * 0.5f, screen.Y - textSize.y - 4.f);
        const ImVec2 dotPos(textPos.x - 7.f, textPos.y + textSize.y * 0.5f);

        draw->AddCircleFilled(dotPos, 3.f, IM_COL32(255, 255, 255, 255), 10);
        DrawOutlinedText(draw, textPos, col, label.c_str());
    }
}

}
