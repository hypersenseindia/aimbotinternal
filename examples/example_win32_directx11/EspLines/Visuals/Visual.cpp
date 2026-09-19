#define IMGUI_DEFINE_MATH_OPERATORS
#include "Visual.hpp"
#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Math/WordToScreen.hpp>
#include <examples/example_win32_directx11/EspLines/Math/Vector/Vector2.hpp>
#include <examples/example_win32_directx11/EspLines/Data/Data.hpp>
#include <examples/example_win32_directx11/src/Fonts/Fonts.hpp>
#include "Namegun.h"
#include <Windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace texture {
    extern ID3D11ShaderResourceView* custom_logo;
}
static bool logoDrawn = false;


bool IsTeam = false;
namespace font
{
    inline ImFont* WeaponsIco = nullptr;

    inline ImFont* icomoon = nullptr;
    inline ImFont* lexend_bold = nullptr;
    inline ImFont* lexend_regular = nullptr;
    inline ImFont* lexend_general_bold = nullptr;

    inline ImFont* icomoon_widget = nullptr;
    inline ImFont* icomoon_widget2 = nullptr;
}

namespace {
bool IsTrainingBotTarget(const Player& player) {
    if (player.Name.empty())
        return true;
    std::string lower = player.Name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return lower.find("training") != std::string::npos;
}

static inline bool EffectsDisabled() {
    return g_Globals.General.DisableAllEffects;
}

static ImU32 FillBoxColorU32(bool isKnocked) {
    if (isKnocked)
        return IM_COL32(255, 0, 0, 45);
    return ImGui::ColorConvertFloat4ToU32(g_Globals.Visuals.fillBoxColor.Value);
}

static void DrawGlowLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImColor color, float thickness, float glowRadius, float feather)
{
    if (EffectsDisabled() || !g_Globals.Visuals.LineGlow || glowRadius <= 0.f) {
        dl->AddLine(a, b, color, thickness);
        return;
    }
    for (float i = glowRadius; i > 0.f; i -= feather) {
        const int alpha = (int)(color.Value.w * 255.f * (i / glowRadius) * 0.15f);
        dl->AddLine(a, b, IM_COL32(
            (int)(color.Value.x * 255.f), (int)(color.Value.y * 255.f),
            (int)(color.Value.z * 255.f), alpha), thickness + i * 0.12f);
    }
    dl->AddLine(a, b, color, thickness);
}

static void DrawGlowBoneLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImColor color, float glowRadius, float feather)
{
    if (EffectsDisabled() || !g_Globals.Visuals.SkeletonGlow || glowRadius <= 0.f) {
        dl->AddLine(a, b, color, 1.0f);
        return;
    }
    for (float i = glowRadius; i > 0.f; i -= feather) {
        const int alpha = (int)(color.Value.w * 255.f * (i / glowRadius) * 0.12f);
        dl->AddLine(a, b, IM_COL32(255, 255, 255, alpha), 1.0f + i * 0.35f);
    }
    dl->AddLine(a, b, color, 1.0f);
}
}

ImColor GetAnimatedRGBColor(float speedMultiplier = 1.0f) {
    float time = ImGui::GetTime() * speedMultiplier;
    float r = (sinf(time + 0.0f) * 0.5f) + 0.5f;
    float g = (sinf(time + 2.0f) * 0.5f) + 0.5f;
    float b = (sinf(time + 4.0f) * 0.5f) + 0.5f;
    return ImColor(r, g, b, 1.0f);
}

void DrawHealthBarVertical(ImDrawList* vList, float x, float y, float w, float h, float healthPercentage, bool isKnocked, float radius = 0.0f, float glowStrength = 6.0f, float border = 0.5f)
{
    healthPercentage = std::clamp(healthPercentage, 0.0f, 1.0f);

    float filledHeight = h * healthPercentage;
    float emptyHeight = h - filledHeight;

    ImVec2 barTopLeft = ImVec2(x, y);
    ImVec2 barBottomRight = ImVec2(x + w, y + h);

    ImVec2 fillTopLeft = ImVec2(x, y + emptyHeight);
    ImVec2 fillBottomRight = ImVec2(x + w, y + h);

    ImColor topColor, bottomColor;

    if (isKnocked) {
        ImVec4 knockCol = g_Globals.Visuals.KnockedEnemiesColor;
        topColor = bottomColor = ImColor(knockCol);
    }
    else if (healthPercentage > 0.75f) {
        topColor = ImColor(34, 112, 18, 255);
        bottomColor = ImColor(51, 160, 27, 255);
    }
    else if (healthPercentage > 0.3f) {
        topColor = ImColor(130, 110, 0, 255);
        bottomColor = ImColor(200, 180, 0, 255);
    }
    else {
        topColor = ImColor(120, 15, 10, 255);
        bottomColor = ImColor(191, 20, 12, 255);
    }

    vList->AddRectFilled(barTopLeft, barBottomRight, IM_COL32(0, 0, 0, 180), radius);

    vList->AddRectFilledMultiColor(
        fillTopLeft, fillBottomRight,
        topColor, topColor, bottomColor, bottomColor
    );

    if (!EffectsDisabled()) {
        float glowSize = 4.0f;
        for (int i = 1; i <= 2; ++i) {
            float offset = glowSize * i;
            ImU32 glowCol = ImColor(
                bottomColor.Value.x,
                bottomColor.Value.y,
                bottomColor.Value.z,
                0.05f / i
            );

            vList->AddRectFilled(
                ImVec2(fillTopLeft.x - offset, fillTopLeft.y - offset),
                ImVec2(fillBottomRight.x + offset, fillBottomRight.y + offset),
                glowCol,
                radius
            );
        }
    }
    vList->AddRect(barTopLeft, barBottomRight, IM_COL32(0, 0, 0, 255), radius, 0, border);
}


void DrawGlowCorneredBox(float x, float y, float w, float h, ImColor color, float thickness = 1.5f, float glowRadius = 12.0f, float feather = 2.0f, bool fillEnabled = true, bool isKnocked = false) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    float newWidth = w;
    float newHeight = h * 1.15f;
    float newY = y - (newHeight - h);

    if (EffectsDisabled()) {
        if (fillEnabled && g_Globals.Visuals.fillBox) {
            drawList->AddRectFilled(
                ImVec2(x, newY),
                ImVec2(x + newWidth, newY + newHeight),
                FillBoxColorU32(isKnocked));
        }
        const float lineW = newWidth / 6.0f;
        const float lineH = newHeight / 6.0f;
        drawList->AddLine(ImVec2(x, newY), ImVec2(x + lineW, newY), color, thickness);
        drawList->AddLine(ImVec2(x, newY), ImVec2(x, newY + lineH), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth - lineW, newY), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth, newY + lineH), color, thickness);
        drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x + lineW, newY + newHeight), color, thickness);
        drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x, newY + newHeight - lineH), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth - lineW, newY + newHeight), color, thickness);
        drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth, newY + newHeight - lineH), color, thickness);
        return;
    }

    if (fillEnabled && g_Globals.Visuals.fillBox) {
        drawList->AddRectFilled(
            ImVec2(x, newY),
            ImVec2(x + newWidth, newY + newHeight),
            FillBoxColorU32(isKnocked));
    }


    for (int i = (int)glowRadius; i > 0; --i) {
        float alphaFactor = 25.0f * (1.0f - ((float)i / glowRadius));
        ImColor glowColor = isKnocked
            ? ImColor(1.f, 0.f, 0.f, alphaFactor / 255.0f)
            : ImColor(color.Value.x, color.Value.y, color.Value.z, alphaFactor / 255.0f);

        float offset = i * 0.5f;
        float sixthW = newWidth / 6.0f;
        float sixthH = newHeight / 6.0f;

        // Top-left
        drawList->AddLine(ImVec2(x - offset, newY - offset), ImVec2(x + sixthW + offset, newY - offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x - offset, newY - offset), ImVec2(x - offset, newY + sixthH + offset), glowColor, thickness);

        // Top-right
        drawList->AddLine(ImVec2(x + newWidth + offset, newY - offset), ImVec2(x + newWidth - sixthW - offset, newY - offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x + newWidth + offset, newY - offset), ImVec2(x + newWidth + offset, newY + sixthH + offset), glowColor, thickness);

        // Bottom-left
        drawList->AddLine(ImVec2(x - offset, newY + newHeight + offset), ImVec2(x + sixthW + offset, newY + newHeight + offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x - offset, newY + newHeight + offset), ImVec2(x - offset, newY + newHeight - sixthH - offset), glowColor, thickness);

        // Bottom-right
        drawList->AddLine(ImVec2(x + newWidth + offset, newY + newHeight + offset), ImVec2(x + newWidth - sixthW - offset, newY + newHeight + offset), glowColor, thickness);
        drawList->AddLine(ImVec2(x + newWidth + offset, newY + newHeight + offset), ImVec2(x + newWidth + offset, newY + newHeight - sixthH - offset), glowColor, thickness);
    }


    float lineW = newWidth / 6.0f;
    float lineH = newHeight / 6.0f;

    // Top-left
    drawList->AddLine(ImVec2(x, newY), ImVec2(x + lineW, newY), color, thickness);
    drawList->AddLine(ImVec2(x, newY), ImVec2(x, newY + lineH), color, thickness);

    // Top-right
    drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth - lineW, newY), color, thickness);
    drawList->AddLine(ImVec2(x + newWidth, newY), ImVec2(x + newWidth, newY + lineH), color, thickness);

    // Bottom-left
    drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x + lineW, newY + newHeight), color, thickness);
    drawList->AddLine(ImVec2(x, newY + newHeight), ImVec2(x, newY + newHeight - lineH), color, thickness);

    // Bottom-right
    drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth - lineW, newY + newHeight), color, thickness);
    drawList->AddLine(ImVec2(x + newWidth, newY + newHeight), ImVec2(x + newWidth, newY + newHeight - lineH), color, thickness);
}
void DrawFullBox(float x, float y, float w, float h, ImColor color, float thickness = 1.5f, bool isKnocked = false) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    float newWidth = w * 0.90f;
    float newHeight = h * 1.17f;
    float newX = x + (w - newWidth) / 2.0f;
    float newY = y - (newHeight - h) + 3.0f; // Slight downward offset


    if (g_Globals.Visuals.fillBox) {
        drawList->AddRectFilled(
            ImVec2(newX, newY),
            ImVec2(newX + newWidth, newY + newHeight),
            FillBoxColorU32(isKnocked));
    }

    drawList->AddRect(
        ImVec2(newX, newY),
        ImVec2(newX + newWidth, newY + newHeight),
        color,
        0.9f,
        ImDrawFlags_None,
        thickness * 0.5f
    );
}
void DrawVerticalHealthBar(short CurrentHealth, short MaxHealth, ImVec2 Position, float TotalHeight, bool isKnocked = false, int GlowRadius = 10) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    const float HealthPercentage = std::clamp(static_cast<float>(CurrentHealth) / MaxHealth, 0.f, 1.f);
    const float FilledBarHeight = TotalHeight * HealthPercentage;
    const float BarWidth = 2.5f;

    ImVec4 HealthBarColor;
    ImVec4 GlowColor;
    if (isKnocked) {
        HealthBarColor = g_Globals.Visuals.KnockedEnemiesColor.Value;
        GlowColor = HealthBarColor;
    }
    else {
        const ImVec4 Green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (HealthPercentage > 0.5f)
            HealthBarColor = ImLerp(Green, Yellow, (1.0f - HealthPercentage) * 2.0f);
        else
            HealthBarColor = ImLerp(Yellow, Red, (0.5f - HealthPercentage) * 2.0f);
        GlowColor = HealthPercentage > 0.5f ? Green : (HealthPercentage > 0.25f ? Yellow : Red);
    }

    const ImU32 BarColorU32 = ImGui::ColorConvertFloat4ToU32(HealthBarColor);

    drawList->AddRectFilled(
        ImVec2(Position.x, Position.y),
        ImVec2(Position.x + BarWidth, Position.y + TotalHeight),
        isKnocked ? IM_COL32(80, 0, 0, 180) : IM_COL32(0, 0, 0, 128)
    );

    if (!EffectsDisabled()) {
        for (int i = GlowRadius; i > 0; --i) {
            const float Alpha = 15.0f * (1.0f - ((float)i / GlowRadius));
            drawList->AddRectFilled(
                ImVec2(Position.x - i, Position.y + (TotalHeight - FilledBarHeight) - i),
                ImVec2(Position.x + BarWidth + i, Position.y + TotalHeight + i),
                IM_COL32((int)(GlowColor.x * 255), (int)(GlowColor.y * 255), (int)(GlowColor.z * 255), (int)Alpha)
            );
        }
    }

    const float fillTop = isKnocked ? Position.y : (Position.y + (TotalHeight - FilledBarHeight));
    const float fillH = isKnocked ? TotalHeight : FilledBarHeight;
    drawList->AddRectFilled(
        ImVec2(Position.x, fillTop),
        ImVec2(Position.x + BarWidth, fillTop + fillH),
        BarColorU32
    );
}

void DrawHealthBarBelowFullBox(short CurrentHealth, short MaxHealth, float x, float y, float w, float h, bool isKnocked = false) {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    const float HealthPercentage = std::clamp(static_cast<float>(CurrentHealth) / MaxHealth, 0.f, 1.f);
    const float FilledBarWidth = isKnocked ? w : (w * HealthPercentage);
    const float BarHeight = 3.5f;
    const float offsetY = 3.5f;
    const float rounding = 0.8f;
    const float outlineSize = 1.5f;
    const int GlowRadius = 8;

    ImVec4 HealthBarColor;
    ImVec4 GlowColor;
    if (isKnocked) {
        HealthBarColor = g_Globals.Visuals.KnockedEnemiesColor.Value;
        GlowColor = HealthBarColor;
    }
    else {
        const ImVec4 Green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        const ImVec4 Red = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (HealthPercentage > 0.5f)
            HealthBarColor = ImLerp(Green, Yellow, (1.0f - HealthPercentage) * 2.0f);
        else
            HealthBarColor = ImLerp(Yellow, Red, (0.5f - HealthPercentage) * 2.0f);
        GlowColor = HealthPercentage > 0.5f ? Green : (HealthPercentage > 0.25f ? Yellow : Red);
    }

    const ImU32 BarColorU32 = ImGui::ColorConvertFloat4ToU32(HealthBarColor);
    ImU32 OutlineColor = IM_COL32(0, 0, 0, 200);


    if (!EffectsDisabled()) {
        for (int i = GlowRadius; i > 0; --i) {
            float Alpha = 15.0f * (1.0f - ((float)i / GlowRadius));
            drawList->AddRectFilled(
                ImVec2(x - i, y + h + offsetY - i),
                ImVec2(x + FilledBarWidth + i, y + h + offsetY + BarHeight + i),
                IM_COL32((int)(GlowColor.x * 255), (int)(GlowColor.y * 255), (int)(GlowColor.z * 255), (int)Alpha),
                rounding
            );
        }
    }


    drawList->AddRect(
        ImVec2(x, y + h + offsetY),
        ImVec2(x + w, y + h + offsetY + BarHeight),
        OutlineColor,
        rounding,
        ImDrawFlags_RoundCornersAll,
        outlineSize
    );


    if (FilledBarWidth > 0) {
        float actualRounding = (FilledBarWidth < rounding * 2) ? FilledBarWidth / 2 : rounding;
        drawList->AddRectFilled(
            ImVec2(x, y + h + offsetY),
            ImVec2(x + FilledBarWidth, y + h + offsetY + BarHeight),
            BarColorU32,
            actualRounding,
            ImDrawFlags_RoundCornersAll
        );


        if (FilledBarWidth < w) {
            drawList->AddRectFilled(
                ImVec2(x + FilledBarWidth - 0.5f, y + h + offsetY),
                ImVec2(x + FilledBarWidth + 0.5f, y + h + offsetY + BarHeight),
                OutlineColor
            );
        }
    }
}
inline bool IsOnScreen(const Vector2& screenPos, int screenWidth, int screenHeight) {
    return screenPos.X >= 0 && screenPos.Y >= 0 && screenPos.X <= screenWidth && screenPos.Y <= screenHeight;
}
namespace {
uint32_t ColorToUint32(const ImVec4& color) {
        return ImColor(color.x * 255, color.y * 255, color.z * 255, color.w * 255);
    }

    uint32_t LerpColor(uint32_t color1, uint32_t color2, float t)
    {
        uint8_t a1 = (color1 >> 24) & 0xFF;
        uint8_t r1 = (color1 >> 16) & 0xFF;
        uint8_t g1 = (color1 >> 8) & 0xFF;
        uint8_t b1 = color1 & 0xFF;

        uint8_t a2 = (color2 >> 24) & 0xFF;
        uint8_t r2 = (color2 >> 16) & 0xFF;
        uint8_t g2 = (color2 >> 8) & 0xFF;
        uint8_t b2 = color2 & 0xFF;

        uint8_t a = static_cast<uint8_t>(a1 + (a2 - a1) * t);
        uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * t);
        uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * t);
        uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * t);

        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    void RenderEspIndicator()
    {
        auto draw = ImGui::GetForegroundDrawList();
        ImVec2 screenCenter = ImVec2(g_Globals.EspConfig.Width / 2.f, g_Globals.EspConfig.Height / 2.f);
        float radius = (float)(std::min(g_Globals.EspConfig.Width, g_Globals.EspConfig.Height)) / 2.0f - 50.0f;

        for (auto& [id, entity] : g_Globals.EspConfig.Entities)
        {
            if (!entity.IsKnown || entity.IsDead || entity.Health <= 0)
                continue;
            if (!g_Globals.EspConfig.showOnlyVisible && !entity.IsVisible)
                continue;

            if (entity.IsTeam == Bool3::True)
                continue;

            Vector2 screenPos = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, entity.Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
            if (screenPos.X >= 0 && screenPos.Y >= 0 &&
                screenPos.X <= g_Globals.EspConfig.Width &&
                screenPos.Y <= g_Globals.EspConfig.Height)
                continue;

            Vector3 dir = entity.Head - g_Globals.EspConfig.MainCamera;

            float angle = atan2f(dir.X, dir.Z) - 0.0f /* ViewYaw */;

            ImVec2 arrowPos = ImVec2(
                screenCenter.x + cosf(angle) * radius,
                screenCenter.y + sinf(angle) * radius
            );

            float size = 14.0f;
            ImVec2 p1 = ImVec2(arrowPos.x + cosf(angle) * size, arrowPos.y + sinf(angle) * size);
            ImVec2 p2 = ImVec2(arrowPos.x + cosf(angle + 2.5f) * size, arrowPos.y + sinf(angle + 2.5f) * size);
            ImVec2 p3 = ImVec2(arrowPos.x + cosf(angle - 2.5f) * size, arrowPos.y + sinf(angle - 2.5f) * size);

            draw->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 0, 0, 255));
        }
    }

} // namespace

namespace ESP {

void Players() {
        if (!g_Globals.Visuals.Enabled || !g_Globals.EspConfig.Matrix)
            return;

        logoDrawn = false;
        ImDrawList* const draw = ImGui::GetBackgroundDrawList();
        if (!draw)
            return;

        Namegun::Init();

        const int screenW = g_Globals.EspConfig.Width;
        const int screenH = g_Globals.EspConfig.Height;

        for (auto& [entityID, player] : g_Globals.EspConfig.Entities) {
            if (!player.IsKnown || player.IsDead) continue;
            if (!g_Globals.EspConfig.showOnlyVisible && !player.IsVisible)
                continue;

            const ImColor knockColor = ImColor(g_Globals.Visuals.KnockedEnemiesColor);
            const bool isKnocked = player.IsKnocked;
            const ImColor rainbow = (!EffectsDisabled() && g_Globals.Visuals.RainbowESP)
                ? GetAnimatedRGBColor(1.0f)
                : ImColor(255, 255, 255, 255);
            auto espColor = [&](ImColor normal) -> ImColor {
                if (isKnocked) return knockColor;
                if (!EffectsDisabled() && g_Globals.Visuals.RainbowESP) return rainbow;
                return normal;
            };

            if (g_Globals.Visuals.IgnoreTrainingBots && IsTrainingBotTarget(player))
                continue;

            float dist = Vector3::Distance(g_Globals.EspConfig.MainCamera, player.Head);
            if (dist > g_Globals.Visuals.RenderDistance) continue;

            Vector2 headPos = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, player.Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
            Vector2 rootPos = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, player.Root, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);

            if (headPos.X < 1.f || headPos.Y < 1.f || rootPos.X < 1.f || rootPos.Y < 1.f)
                continue;
            if (headPos.X > screenW || headPos.Y > screenH)
                continue;

            float boxHeight = fabsf(headPos.Y - rootPos.Y);
            float boxWidth = boxHeight * 0.65f;
            float x = headPos.X - (boxWidth / 2), y = headPos.Y - 11;
            float w = boxWidth, h = boxHeight + 17;;

            ImColor currecntlinecolor = espColor(ImColor(g_Globals.Visuals.SnapLinesColor));
            ImColor currentBoxColor = espColor(ImColor(g_Globals.Visuals.BoxColor));
            ImColor currentSkeletonColor = espColor(ImColor(g_Globals.Visuals.SkeletonColor));
            ImColor currentNameColor = espColor(ImColor(g_Globals.Visuals.NameColor));
            ImColor currentDistanceColor = espColor(ImColor(g_Globals.Visuals.DistanceColor));
            ImColor currentRankColor = espColor(ImColor(g_Globals.Visuals.RankColor));
            ImColor weaponcolorxd = espColor(ImColor(g_Globals.Visuals.WeaponColor));

            if (g_Globals.Visuals.Lines) {
                const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
                const float logoDim = std::max(16.f, g_Globals.Visuals.EspLineLogoSize);
                const ImVec2 logoSize(logoDim, logoDim);
                const float padding = 12.0f;
                ImVec2 logoPos;
                ImVec2 logoCenter;

                switch (g_Globals.Visuals.EspLines) {
                case 0:
                    logoPos = ImVec2((screenSize.x * 0.5f) - (logoSize.x * 0.5f), padding);
                    break;
                case 1:
                    logoPos = ImVec2(
                        (screenSize.x * 0.5f) - (logoSize.x * 0.5f),
                        (screenSize.y * 0.5f) - (logoSize.y * 0.5f));
                    break;
                case 2:
                default:
                    logoPos = ImVec2(
                        (screenSize.x * 0.5f) - (logoSize.x * 0.5f),
                        screenSize.y - logoSize.y - padding);
                    break;
                }

                logoCenter = ImVec2(logoPos.x + logoSize.x * 0.5f, logoPos.y + logoSize.y * 0.5f);

                if (g_Globals.Visuals.LineGlow && !EffectsDisabled()) {
                    DrawGlowLine(draw, ImVec2(headPos.X, headPos.Y), logoCenter, currecntlinecolor, 1.2f,
                        g_Globals.Visuals.LineGlowRadius, 2.f);
                }
                else {
                    draw->AddLine(ImVec2(headPos.X, headPos.Y), logoCenter, currecntlinecolor, 1.2f);
                }

                if (g_Globals.Visuals.ShowLogo != 0 && !logoDrawn && texture::custom_logo) {
                    if (!EffectsDisabled()) {
                        draw->AddImage(
                            (ImTextureID)texture::custom_logo,
                            logoPos - ImVec2(2, 2),
                            logoPos + logoSize + ImVec2(2, 2),
                            ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32(255, 255, 255, 90));
                    }
                    draw->AddImage(
                        (ImTextureID)texture::custom_logo,
                        logoPos,
                        logoPos + logoSize);
                    logoDrawn = true;
                }
            }




            /*   if (g_Globals.Visuals.Box) {
                   auto vList = ImGui::GetForegroundDrawList();

                   switch (g_Globals.Visuals.players_box) {
                   case 0:
                   {
                       ImVec2 boxStart = ImVec2(x, y);
                       ImVec2 boxEnd = ImVec2(x + w, y + h);


                       vList->AddRect(boxStart, boxEnd, currentBoxColor, 1.5f, 0, 1.0f);


                       if (g_Globals.Visuals.ShinyEffects) {

                       }
                       break;
                   }

                   case 1:
                   {
                       float XBox = headPos.X - ((fabsf(headPos.Y - rootPos.Y) * 0.62f) / 2.0f);
                       float YBox = headPos.Y;
                       float WBox = fabsf(headPos.Y - rootPos.Y) * 0.60f;
                       float HBox = fabsf(headPos.Y - rootPos.Y);
                       float thickness = 1.0f;

                       float lineW = WBox / 3;
                       float lineH = HBox / 3;


                       vList->AddLine(ImVec2(XBox, YBox), ImVec2(XBox, YBox + lineH), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox, YBox), ImVec2(XBox + lineW, YBox), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox + WBox - lineW, YBox), ImVec2(XBox + WBox, YBox), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox + WBox, YBox), ImVec2(XBox + WBox, YBox + lineH), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox, YBox + HBox - lineH), ImVec2(XBox, YBox + HBox), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox, YBox + HBox), ImVec2(XBox + lineW, YBox + HBox), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox + WBox - lineW, YBox + HBox), ImVec2(XBox + WBox, YBox + HBox), currentBoxColor, thickness);
                       vList->AddLine(ImVec2(XBox + WBox, YBox + HBox - lineH), ImVec2(XBox + WBox, YBox + HBox), currentBoxColor, thickness);


                       if (g_Globals.Visuals.ShinyEffects) {
                           ImVec2 cornerLines[8][2] = {
                               { ImVec2(XBox, YBox), ImVec2(XBox, YBox + lineH) },
                               { ImVec2(XBox, YBox), ImVec2(XBox + lineW, YBox) },
                               { ImVec2(XBox + WBox - lineW, YBox), ImVec2(XBox + WBox, YBox) },
                               { ImVec2(XBox + WBox, YBox), ImVec2(XBox + WBox, YBox + lineH) },
                               { ImVec2(XBox, YBox + HBox - lineH), ImVec2(XBox, YBox + HBox) },
                               { ImVec2(XBox, YBox + HBox), ImVec2(XBox + lineW, YBox + HBox) },
                               { ImVec2(XBox + WBox - lineW, YBox + HBox), ImVec2(XBox + WBox, YBox + HBox) },
                               { ImVec2(XBox + WBox, YBox + HBox - lineH), ImVec2(XBox + WBox, YBox + HBox) }
                           };

                           for (int i = 0; i < 8; ++i) {

                           }
                       }
                       break;
                   }
                   }
               }*/

            if (g_Globals.Visuals.Box) {

                const float glowRadius = g_Globals.Visuals.BoxGlow ? g_Globals.Visuals.BoxGlowRadius : 0.f;
                const float feather = 2.0f;

                switch (g_Globals.Visuals.players_box) {
                case 0:
                    DrawFullBox(headPos.X - (boxWidth / 2), headPos.Y - 1.0f, boxWidth, boxHeight, currentBoxColor, 0.5f, isKnocked);
                    break;
                case 1:
                default:
                    DrawGlowCorneredBox(headPos.X - (boxWidth / 2), headPos.Y, boxWidth, boxHeight, currentBoxColor, 0.5f, glowRadius, feather, g_Globals.Visuals.fillBox, isKnocked);
                    break;
                }
            }


            ImVec2 headPosition = W2S::WorldToScreenImVec2(g_Globals.EspConfig.ViewMatrix, player.Head, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height); // Head

            if (g_Globals.Visuals.HealthBar) {
                const float scaledBoxHeight = boxHeight * 1.15f;
                const float boxTopY = headPosition.y - 1.0f - (scaledBoxHeight - boxHeight);
                const float boxLeftX = headPosition.x - (boxWidth / 2);
                const float widerBoxWidth = boxWidth * 1.1f;

                switch (g_Globals.Visuals.players_healthbar) {
                case 0:
                    DrawVerticalHealthBar(player.Health, 200, ImVec2(boxLeftX - 5, boxTopY), scaledBoxHeight, isKnocked);
                    break;
                case 1:
                    DrawVerticalHealthBar(player.Health, 200, ImVec2(boxLeftX + boxWidth + 5, boxTopY), scaledBoxHeight, isKnocked);
                    break;
                case 2:
                    DrawHealthBarBelowFullBox(player.Health, 200, headPosition.x - (widerBoxWidth / 2), headPosition.y, widerBoxWidth, boxHeight, isKnocked);
                    break;
                case 3:
                    DrawHealthBarBelowFullBox(player.Health, 200, headPosition.x - (widerBoxWidth / 2), headPosition.y - scaledBoxHeight - 5, widerBoxWidth, boxHeight, isKnocked);
                    break;
                }
            }

            float offsetTop = 5.0f;
            float offsetBottom = 5.0f;
            float offsetLeftX = 5.0f;
            float offsetLeftY = 0.0f;
            float offsetRightX = 5.0f;
            float offsetRightY = 0.0f;

            if (g_Globals.Visuals.HealthBar) {
                if (g_Globals.Visuals.players_healthbar == 0) offsetLeftX += 6.0f;
                if (g_Globals.Visuals.players_healthbar == 1) offsetRightX += 6.0f;
                if (g_Globals.Visuals.players_healthbar == 2) offsetBottom += 6.0f;
                if (g_Globals.Visuals.players_healthbar == 3) offsetTop += 6.0f;
            }

            auto GetTextPosition = [&](int side, ImVec2 textSize) -> ImVec2 {
                ImVec2 pos;
                switch (side) {
                case 0:
                    pos.x = headPosition.x - (boxWidth / 2) - textSize.x - offsetLeftX;
                    pos.y = headPosition.y + offsetLeftY;
                    offsetLeftY += textSize.y + 2.0f;
                    break;
                case 1:
                    pos.x = headPosition.x + (boxWidth / 2) + offsetRightX;
                    pos.y = headPosition.y + offsetRightY;
                    offsetRightY += textSize.y + 2.0f;
                    break;
                case 2:
                    pos.x = headPosition.x - (textSize.x / 2);
                    pos.y = headPosition.y - textSize.y - offsetTop;
                    offsetTop += textSize.y + 2.0f;
                    break;
                case 3:
                    pos.x = headPosition.x - (textSize.x / 2);
                    pos.y = headPosition.y + boxHeight + offsetBottom;
                    offsetBottom += textSize.y + 2.0f;
                    break;
                default:
                    pos = headPosition;
                    break;
                }
                return pos;
            };

            auto DrawOutlinedText = [&](ImFont* font, const ImVec2& pos, ImColor color, const char* text) {
                if (!text || !text[0]) return;
                const float fs = font ? font->FontSize : ImGui::GetFontSize();
                const ImU32 outline = IM_COL32(0, 0, 0, 200);
                constexpr float outlineSize = 1.0f;
                for (float ox = -outlineSize; ox <= outlineSize; ox += outlineSize) {
                    for (float oy = -outlineSize; oy <= outlineSize; oy += outlineSize) {
                        if (ox == 0.f && oy == 0.f) continue;
                        draw->AddText(font, fs, ImVec2(pos.x + ox, pos.y + oy), outline, text);
                    }
                }
                draw->AddText(font, fs, pos, color, text);
            };

            if (g_Globals.Visuals.Name) {
                const std::string nameText = player.Name.empty() ? "BOT - Training" : player.Name;
                ImFont* nameFont =
                    FWork::Fonts::NotoSansRegular ? FWork::Fonts::NotoSansRegular :
                    FWork::Fonts::NotoSansSymbolsRegular ? FWork::Fonts::NotoSansSymbolsRegular :
                    FWork::Fonts::NotoEmojiRegular ? FWork::Fonts::NotoEmojiRegular :
                    FWork::Fonts::LexendRegular ? FWork::Fonts::LexendRegular :
                    FWork::Fonts::ArialUnicode ? FWork::Fonts::ArialUnicode :
                    FWork::Fonts::InterBold ? FWork::Fonts::InterBold :
                    ImGui::GetFont();
                if (nameFont) ImGui::PushFont(nameFont);
                const ImVec2 textSize = ImGui::CalcTextSize(nameText.c_str());
                if (nameFont) ImGui::PopFont();
                const ImVec2 namePos = GetTextPosition(g_Globals.Visuals.EspNameSide, textSize);
                const float nameFs = nameFont ? nameFont->FontSize : ImGui::GetFontSize();
                if (!EffectsDisabled()) {
                    for (float ox = -1.f; ox <= 1.f; ox += 1.f) {
                        for (float oy = -1.f; oy <= 1.f; oy += 1.f) {
                            if (ox == 0.f && oy == 0.f) continue;
                            draw->AddText(nameFont, nameFs, ImVec2(namePos.x + ox, namePos.y + oy), IM_COL32(0, 0, 0, 200), nameText.c_str());
                        }
                    }
                    draw->AddText(nameFont, nameFs, ImVec2(namePos.x + 0.65f, namePos.y), currentNameColor, nameText.c_str());
                    draw->AddText(nameFont, nameFs, namePos, currentNameColor, nameText.c_str());
                } else {
                    DrawOutlinedText(nameFont, namePos, currentNameColor, nameText.c_str());
                }
            }

            if (g_Globals.Visuals.Distance) {
                const std::string distanceText = std::to_string((int)std::round(dist)) + "m";
                ImFont* distFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                if (distFont) ImGui::PushFont(distFont);
                const ImVec2 textSize = ImGui::CalcTextSize(distanceText.c_str());
                if (distFont) ImGui::PopFont();
                const ImVec2 distPos = GetTextPosition(g_Globals.Visuals.EspDistanceSide, textSize);
                DrawOutlinedText(distFont, distPos, currentDistanceColor, distanceText.c_str());
            }

            if (g_Globals.Visuals.Rank && !player.RankText.empty()) {
                ImFont* rankFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                if (rankFont) ImGui::PushFont(rankFont);
                const ImVec2 textSize = ImGui::CalcTextSize(player.RankText.c_str());
                if (rankFont) ImGui::PopFont();
                const ImVec2 rankPos = GetTextPosition(g_Globals.Visuals.EspRankSide, textSize);
                DrawOutlinedText(rankFont, rankPos, currentRankColor, player.RankText.c_str());
            }

            if (g_Globals.Visuals.WeaponName && Namegun::ShouldShowWeapon(player.Gun)) {
                const std::string gunName = Namegun::GetGunName(player.Gun);
                if (!gunName.empty()) {
                    ImFont* gunFont = FWork::Fonts::InterBold ? FWork::Fonts::InterBold : ImGui::GetFont();
                    if (gunFont) ImGui::PushFont(gunFont);
                    const ImVec2 textSize = ImGui::CalcTextSize(gunName.c_str());
                    if (gunFont) ImGui::PopFont();
                    const ImVec2 gunPos = GetTextPosition(g_Globals.Visuals.EspWeaponTextSide, textSize);
                    DrawOutlinedText(gunFont, gunPos, weaponcolorxd, gunName.c_str());
                }
            }

            if (g_Globals.Visuals.WeaponIcon && Namegun::ShouldShowWeapon(player.Gun) && Namegun::HasIcon(player.Gun)) {
                std::string iconText = Namegun::GetGunIcon(player.Gun);
                const std::string fullName = Namegun::GetGunName(player.Gun);
                const std::string baseName = Namegun::GetBaseName(fullName);
                if (baseName != fullName) {
                    const std::string suffix = fullName.substr(baseName.length());
                    const size_t iconSuffixPos = iconText.find(suffix);
                    if (iconSuffixPos != std::string::npos)
                        iconText = iconText.substr(0, iconSuffixPos);
                }

                ImFont* iconFont = FWork::Fonts::IconWeapon ? FWork::Fonts::IconWeapon : ImGui::GetFont();
                if (iconFont && !iconText.empty()) {
                    const float iconFontSize = iconFont->FontSize;
                    const ImVec2 iconSize = iconFont->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, iconText.c_str());
                    const ImVec2 iconPos = GetTextPosition(g_Globals.Visuals.EspWeaponIconSide, iconSize);
                    DrawOutlinedText(iconFont, iconPos, weaponcolorxd, iconText.c_str());
                }
            }

            if (g_Globals.Visuals.Skeleton) {
                    auto DrawBone = [&](const Vector3& from, const Vector3& to) {
                        Vector2 screenFrom = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, from, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
                        Vector2 screenTo = W2S::WorldToScreen(g_Globals.EspConfig.ViewMatrix, to, g_Globals.EspConfig.Width, g_Globals.EspConfig.Height);
                        if (screenFrom.X >= 1.f && screenFrom.Y >= 1.f && screenTo.X >= 1.f && screenTo.Y >= 1.f)
                            DrawGlowBoneLine(draw, ImVec2(screenFrom.X, screenFrom.Y), ImVec2(screenTo.X, screenTo.Y),
                                currentSkeletonColor, g_Globals.Visuals.SkeletonGlowRadius, 2.f);
                        };
                    DrawBone(player.Head, player.Neck);
                    DrawBone(player.Neck, player.Hip);
                    DrawBone(player.Neck, player.LeftShoulder);
                    DrawBone(player.LeftShoulder, player.LeftElbow);
                    DrawBone(player.LeftElbow, player.LeftWrist);
                    DrawBone(player.Neck, player.RightShoulder);
                    DrawBone(player.RightShoulder, player.RightElbow);
                    DrawBone(player.RightElbow, player.RightWrist);
                    DrawBone(player.Hip, player.Groin);
                    DrawBone(player.Groin, player.LeftAnkle);
                    DrawBone(player.Groin, player.RightAnkle);
                    DrawBone(player.LeftAnkle, player.LeftFoot);
                    DrawBone(player.RightAnkle, player.RightFoot);
                }
        }

        if (g_Globals.Visuals.SnapLines) {
            Player* closest = nullptr;
            float closestDist = FLT_MAX;

            for (auto& [id, scan] : g_Globals.EspConfig.Entities) {
                if (!scan.IsKnown || scan.IsDead)
                    continue;
                if (!g_Globals.EspConfig.showOnlyVisible && !scan.IsVisible)
                    continue;
                if (g_Globals.Visuals.IgnoreTrainingBots && IsTrainingBotTarget(scan))
                    continue;
                if (g_Globals.AimBot.IgnoreKnocked && scan.IsKnocked)
                    continue;

                const float d = Vector3::Distance(g_Globals.EspConfig.MainCamera, scan.Head);
                if (d >= closestDist || d >= 20.0f)
                    continue;

                const Vector2 screenPos = W2S::WorldToScreen(
                    g_Globals.EspConfig.ViewMatrix, scan.Head, screenW, screenH);
                if (screenPos.X < 1.f || screenPos.Y < 1.f)
                    continue;

                closestDist = d;
                closest = &scan;
            }

            if (closest) {
                const Vector2 screenPos = W2S::WorldToScreen(
                    g_Globals.EspConfig.ViewMatrix, closest->Head, screenW, screenH);
                if (screenPos.X >= 1.f && screenPos.Y >= 1.f) {
                    const ImVec2 center(screenW * 0.5f, screenH * 0.5f);
                    const ImColor lineColor = closest->IsKnocked
                        ? ImColor(g_Globals.Visuals.KnockedEnemiesColor)
                        : ImColor(g_Globals.Visuals.OriginLineColor);
                    draw->AddLine(center, ImVec2(screenPos.X, screenPos.Y), lineColor, 1.0f);
                }
            }
        }

    if (g_Globals.Visuals.InvalidTimer && !EffectsDisabled()) {
        FWork::Data::DrawInvalidMatchTimer(
            static_cast<float>(g_Globals.EspConfig.Width),
            static_cast<float>(g_Globals.EspConfig.Height));
    }
}

} // namespace ESP

