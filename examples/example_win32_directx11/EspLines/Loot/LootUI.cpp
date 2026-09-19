#include "LootUI.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_settings.h>
#include <examples/example_win32_directx11/ImGui/custom_widgets.hpp>
#include <examples/example_win32_directx11/ImGui/font_defines.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Loot/LootDatabase.hpp>

namespace {
    static char s_search[96] = "";
    static int s_category = 0;

    static constexpr float kPanelW = 288.f;
    static constexpr float kHintH = 24.f;
    static constexpr float kRowH = 30.f;
    static constexpr float kSearchH = 34.f;
    static constexpr float kComboH = 32.f;
    static constexpr float kBtnH = 30.f;
    static constexpr float kHeaderH = 22.f;
    static constexpr float kCategorySideW = 228.f;
    static constexpr float kCategoryRowH = 30.f;
    static constexpr float kSideGap = 8.f;
    static constexpr float kPanelRound = 12.f;
    static constexpr float kElemRound = 7.f;
    static constexpr float kTitleSize = 14.f;
    static constexpr float kCheckSz = 18.f;
    static constexpr float kBorderThick = 1.75f;

    // Dark ash base + cool cyan accent (modern, not flat grey).
    static constexpr ImVec4 kAshBg{0.08f, 0.085f, 0.10f, 0.98f};
    static constexpr ImVec4 kAshBgTop{0.11f, 0.12f, 0.14f, 0.55f};
    static constexpr ImVec4 kAshFrame{0.13f, 0.14f, 0.16f, 1.f};
    static constexpr ImVec4 kAshHover{0.18f, 0.20f, 0.23f, 1.f};
    static constexpr ImVec4 kAshSelect{0.22f, 0.25f, 0.29f, 1.f};
    static constexpr ImVec4 kAshStroke{0.42f, 0.44f, 0.48f, 0.55f};
    static constexpr ImVec4 kAshScroll{0.30f, 0.32f, 0.35f, 0.50f};
    static constexpr ImVec4 kAccent{0.35f, 0.78f, 0.98f, 1.f};
    static constexpr ImVec4 kAccentSoft{0.35f, 0.78f, 0.98f, 0.18f};
    static constexpr ImVec4 kAccentFill{0.30f, 0.68f, 0.88f, 1.f};
    static constexpr ImVec4 kAccentGlow{0.45f, 0.85f, 1.00f, 0.40f};

    static ImU32 ThemeU32(const ImColor& col, float alpha = 1.f) {
        return ImGui::GetColorU32(ImVec4(col.Value.x, col.Value.y, col.Value.z, col.Value.w * alpha));
    }

    static ImU32 AshU32(ImVec4 col, float alpha = 1.f) {
        col.w *= ImClamp(alpha, 0.f, 1.f);
        return ImGui::GetColorU32(col);
    }

    static float EaseOutQuad(float t) {
        t = ImClamp(t, 0.f, 1.f);
        return 1.f - (1.f - t) * (1.f - t);
    }

    static float EaseOutCubic(float t) {
        t = ImClamp(t, 0.f, 1.f);
        return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
    }

    static ImU32 AccentU32(float alpha = 1.f, const ImVec4* tint = nullptr) {
        const ImVec4 c = tint ? *tint : kAccent;
        return AshU32(c, alpha);
    }

    static ImU32 LerpColor(ImU32 a, ImU32 b, float t) {
        const ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
        const ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
        return ImGui::ColorConvertFloat4ToU32(ImLerp(ca, cb, ImClamp(t, 0.f, 1.f)));
    }

    static ImU32 WithAlpha(ImU32 col, float alpha) {
        ImVec4 v = ImGui::ColorConvertU32ToFloat4(col);
        v.w *= ImClamp(alpha, 0.f, 1.f);
        return ImGui::ColorConvertFloat4ToU32(v);
    }

    static void DrawPanelChrome(ImDrawList* dl, const ImVec2& wmin, const ImVec2& wmax, float alpha) {
        const float h = wmax.y - wmin.y;
        dl->AddRectFilledMultiColor(
            wmin, ImVec2(wmax.x, wmin.y + ImMin(48.f, h * 0.22f)),
            AshU32(kAshBgTop, alpha),
            AshU32(kAshBgTop, alpha),
            AshU32(kAshBg, 0.f),
            AshU32(kAshBg, 0.f));

        dl->AddRectFilled(
            ImVec2(wmin.x + 1.f, wmin.y + 10.f),
            ImVec2(wmin.x + 3.5f, wmax.y - 10.f),
            AccentU32(alpha * 0.9f), 2.f);

        dl->AddRect(wmin, wmax, AshU32(kAshStroke, alpha), kPanelRound, 0, kBorderThick);
        dl->AddRect(
            ImVec2(wmin.x + 0.5f, wmin.y + 0.5f),
            ImVec2(wmax.x - 0.5f, wmax.y - 0.5f),
            AccentU32(alpha * 0.22f), kPanelRound, 0, 1.f);
    }

    static void DrawFillCheckbox(ImDrawList* dl, const ImRect& rect, float fillAnim, float hoverAnim, float alpha) {
        const float pad = 1.5f;
        const ImVec2 boxMin = rect.Min + ImVec2(pad, pad);
        const ImVec2 boxMax = rect.Max - ImVec2(pad, pad);
        const float round = 5.f;

        const ImU32 bgOff = AshU32(ImVec4(0.10f, 0.11f, 0.13f, 1.f), alpha);
        const ImU32 bgOn = AccentU32(alpha, &kAccentFill);
        const ImU32 borderOff = AshU32(ImVec4(0.50f, 0.53f, 0.58f, 0.95f), alpha);
        const ImU32 borderOn = AccentU32(alpha);
        const ImU32 glow = AccentU32(alpha * fillAnim * hoverAnim * 0.5f, &kAccentGlow);

        dl->AddRectFilled(boxMin, boxMax, LerpColor(bgOff, bgOn, fillAnim), round);

        if (fillAnim > 0.05f) {
            const float inset = 3.f + (1.f - fillAnim) * 2.f;
            dl->AddRectFilled(
                boxMin + ImVec2(inset, inset),
                boxMax - ImVec2(inset, inset),
                WithAlpha(glow, fillAnim), round - 2.f);
        }

        const float borderMix = ImMax(fillAnim, hoverAnim * 0.7f);
        dl->AddRect(boxMin, boxMax, LerpColor(borderOff, borderOn, borderMix), round, 0, kBorderThick);
    }

    struct RowAnimState {
        float enabled = 0.f;
        float hover = 0.f;
        float check = 0.f;
        float press = 0.f;
    };

    static std::unordered_map<uint32_t, RowAnimState> s_rowAnim;
    static float s_panelAlpha = 0.f;
    static float s_counterAnim = 0.f;
    static bool s_categoryOpen = false;
    static float s_categoryOpenAnim = 0.f;
    static ImVec2 s_panelPos = ImVec2(0, 0);
    static ImVec2 s_panelSize = ImVec2(0, 0);
    static ImRect s_categoryTriggerRect;
    static ImRect s_categorySideRect;
    static bool s_embeddedMode = false;

    static float PanelAlpha() {
        return EaseOutCubic(s_panelAlpha);
    }

    static ImFont* SafeFont(ImFont* font) {
        if (font)
            return font;
        ImFont* def = ImGui::GetFont();
        if (def)
            return def;
        ImGuiIO& io = ImGui::GetIO();
        return io.Fonts && io.Fonts->Fonts.Size > 0 ? io.Fonts->Fonts[0] : nullptr;
    }

    static ImFont* ItemFont() {
        return SafeFont(font::inter_medium);
    }

    static void DrawFieldLabel(const char* text, float alpha) {
        ImFont* labelFont = SafeFont(font::small_font);
        if (labelFont)
            ImGui::PushFont(labelFont);
        ImGui::TextColored(ImVec4(0.72f, 0.78f, 0.86f, alpha), "%s", text);
        if (labelFont)
            ImGui::PopFont();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
    }

    static float AnimStep(float current, float target, float speed = 12.f) {
        const float dt = ImClamp(ImGui::GetIO().DeltaTime * speed, 0.008f, 0.35f);
        return ImLerp(current, target, dt);
    }

    static ImU32 UiTextColor(bool selected, bool hovered, float alpha) {
        const ImColor col = selected
            ? c::text::label::active
            : (hovered ? c::text::label::hovered : c::text::label::regular);
        return ThemeU32(col, alpha);
    }

    static ImU32 ItemRowTextColor(const ImVec4& itemColor, float enabledAnim, bool hovered, float alpha) {
        ImU32 offCol = AshU32(ImVec4(0.62f, 0.64f, 0.68f, 1.f), alpha);
        ImU32 onCol = AshU32(ImVec4(
            ImLerp(0.88f, itemColor.x, 0.35f),
            ImLerp(0.90f, itemColor.y, 0.35f),
            ImLerp(0.92f, itemColor.z, 0.35f), 1.f), alpha);
        ImU32 col = LerpColor(offCol, onCol, ImClamp(enabledAnim, 0.f, 1.f));
        if (hovered)
            col = LerpColor(col, onCol, 0.28f);
        return col;
    }

    static bool PassesSearch(const LootCatalogItem& item) {
        if (!s_search[0])
            return true;
        std::string nameLower = item.name;
        std::string q = s_search;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(q.begin(), q.end(), q.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return nameLower.find(q) != std::string::npos;
    }

    static void CollectVisible(std::vector<const LootCatalogItem*>& out) {
        out.clear();
        const bool globalSearch = s_search[0] != '\0';
        for (const LootCatalogItem& item : LootDatabase::Catalog()) {
            if (!globalSearch && item.category != s_category)
                continue;
            if (!PassesSearch(item))
                continue;
            out.push_back(&item);
        }
    }

    static RowAnimState& RowAnim(uint32_t id) {
        return s_rowAnim[id];
    }

    static void PruneRowAnim(const std::vector<const LootCatalogItem*>& visible) {
        if (s_rowAnim.size() < 400)
            return;
        std::unordered_map<uint32_t, RowAnimState> keep;
        keep.reserve(visible.size());
        for (const LootCatalogItem* item : visible)
            keep[item->id] = s_rowAnim[item->id];
        s_rowAnim.swap(keep);
    }

    static bool DrawAshButton(const char* id, const char* label, const ImVec2& size, float alpha, bool primary = false) {
        ImGui::PushID(id);
        const bool clicked = ImGui::InvisibleButton("##ash_btn", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();

        const ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 bg = AshU32(kAshFrame, alpha);
        if (held)
            bg = AshU32(kAshSelect, alpha);
        else if (hovered)
            bg = LerpColor(AshU32(kAshHover, alpha), AccentU32(alpha * 0.35f, &kAccentSoft), primary ? 0.55f : 0.35f);

        dl->AddRectFilled(r.Min, r.Max, bg, kElemRound);
        const ImU32 border = hovered
            ? LerpColor(AshU32(kAshStroke, alpha), AccentU32(alpha * 0.75f), primary ? 0.7f : 0.45f)
            : AshU32(kAshStroke, alpha);
        dl->AddRect(r.Min, r.Max, border, kElemRound, 0, kBorderThick);

        ImFont* btnFont = SafeFont(font::small_font);
        if (!btnFont)
            btnFont = ItemFont();
        if (!btnFont) {
            ImGui::PopID();
            return clicked;
        }
        const float fs = btnFont->FontSize;
        const ImVec2 ts = btnFont->CalcTextSizeA(fs, FLT_MAX, 0.f, label);
        const ImVec2 tp(
            r.Min.x + (size.x - ts.x) * 0.5f,
            r.Min.y + (size.y - ts.y) * 0.5f);
        const ImU32 textCol = hovered
            ? LerpColor(ThemeU32(c::text::label::active, alpha), AccentU32(alpha), 0.25f)
            : ThemeU32(c::text::label::active, alpha);
        dl->AddText(btnFont, fs, tp, textCol, label);

        ImGui::PopID();
        return clicked;
    }

    static void DrawCategoryTrigger(int category, int catCount, float alpha) {
        DrawFieldLabel("Category", alpha);

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float rowW = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowMax(rowMin.x + rowW, rowMin.y + kComboH);
        s_categoryTriggerRect = ImRect(rowMin, rowMax);

        ImGui::InvisibleButton("##loot_category_trigger", ImVec2(rowW, kComboH));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            if (s_embeddedMode)
                ImGui::OpenPopup("##loot_cat_drop");
            else
                s_categoryOpen = !s_categoryOpen;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(rowMin, rowMax, AshU32(kAshFrame, alpha), kElemRound);
        const float openMix = s_categoryOpenAnim;
        const ImU32 borderCol = LerpColor(
            AshU32(kAshStroke, alpha),
            AccentU32(alpha),
            ImMax((hovered ? 0.5f : 0.f), openMix * 0.85f));
        dl->AddRect(rowMin, rowMax, borderCol, kElemRound, 0, kBorderThick);
        const bool catOpen = s_embeddedMode ? ImGui::IsPopupOpen("##loot_cat_drop") : s_categoryOpen;
        if (catOpen || hovered)
            dl->AddRectFilled(rowMin, ImVec2(rowMin.x + 2.5f, rowMax.y), AccentU32(alpha * 0.55f), 2.f);

        const char* preview = LootDatabase::CategoryName(category);
        ImFont* textFont = ItemFont();
        if (!textFont)
            return;
        const float textSize = textFont->FontSize;
        const ImVec2 previewDim = textFont->CalcTextSizeA(textSize, FLT_MAX, 0.f, preview);
        const ImVec2 textPos(
            rowMin.x + 10.f,
            rowMin.y + (kComboH - previewDim.y) * 0.5f);
        dl->AddText(textFont, textSize, textPos, UiTextColor(true, hovered, alpha), preview);

        if (font::icomoon_page) {
            const ImVec2 chevSize = font::icomoon_page->CalcTextSizeA(12.f, FLT_MAX, 0.f, ICON_DOWN_SMALL_LINE);
            const ImVec2 chevPos(
                rowMax.x - chevSize.x - 10.f,
                rowMin.y + (kComboH - chevSize.y) * 0.5f);
            dl->AddText(font::icomoon_page, 12.f, chevPos,
                ThemeU32(c::text::description::regular, alpha), ICON_DOWN_SMALL_LINE);
        }
    }

    static void DrawCategorySidePanel(int* category, int catCount, float alpha) {
        s_categoryOpenAnim = AnimStep(s_categoryOpenAnim, s_categoryOpen ? 1.f : 0.f, 16.f);
        if (s_categoryOpenAnim < 0.01f && !s_categoryOpen)
            return;

        const float listH = catCount * kCategoryRowH + 12.f;
        const float maxH = s_panelSize.y - (s_categoryTriggerRect.Min.y - s_panelPos.y) - 4.f;
        const float sideH = ImMin(ImMax(listH, 120.f), ImMax(maxH, 160.f));

        // Always open to the right of the main loot panel.
        ImVec2 sidePos(
            s_panelPos.x + s_panelSize.x + kSideGap,
            s_categoryTriggerRect.Min.y);

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2 vpMax = vp->Pos + vp->Size;
        if (sidePos.y + sideH > vpMax.y - 4.f)
            sidePos.y = vpMax.y - sideH - 4.f;
        if (sidePos.y < vp->Pos.y + 4.f)
            sidePos.y = vp->Pos.y + 4.f;

        ImGui::SetNextWindowPos(sidePos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kCategorySideW, sideH), ImGuiCond_Always);

        const float winAlpha = alpha * EaseOutCubic(s_categoryOpenAnim);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, winAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kPanelRound);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kElemRound);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kAshBg);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.f, 0.f, 0.f, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, kAshScroll);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.38f, 0.38f, 0.40f, 0.65f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.45f, 0.45f, 0.47f, 0.75f));

        const ImGuiWindowFlags sideFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing;

        if (ImGui::Begin("##LootCategorySide", nullptr, sideFlags)) {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            s_categorySideRect = ImGui::GetCurrentWindow()->Rect();

            ImDrawList* wdl = ImGui::GetWindowDrawList();
            const ImVec2 wmin = ImGui::GetWindowPos();
            const ImVec2 wmax = wmin + ImGui::GetWindowSize();
            DrawPanelChrome(wdl, wmin, wmax, winAlpha);

            ImGui::PushFont(font::small_font);
            ImGui::TextColored(ImVec4(0.62f, 0.62f, 0.65f, winAlpha), "Categories");
            ImGui::PopFont();
            ImGui::Spacing();

            ImGui::BeginChild("##loot_cat_scroll", ImVec2(-1.f, -1.f), false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);

            ImFont* rowFont = ItemFont();
            const float rowFontSize = rowFont->FontSize;

            for (int i = 0; i < catCount; ++i) {
                ImGui::PushID(i);
                const bool selected = (*category == i);
                ImGui::InvisibleButton("##cat_row", ImVec2(-1.f, kCategoryRowH));
                const bool rowHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    *category = i;
                    s_categoryOpen = false;
                }

                const ImRect rr(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                ImDrawList* dl = ImGui::GetWindowDrawList();

                if (selected) {
                    dl->AddRectFilled(rr.Min, rr.Max, AshU32(kAshSelect, winAlpha), kElemRound);
                    dl->AddRectFilled(rr.Min, ImVec2(rr.Min.x + 3.f, rr.Max.y), AccentU32(winAlpha * 0.85f), 2.f);
                }
                else if (rowHovered)
                    dl->AddRectFilled(rr.Min, rr.Max, AshU32(kAshHover, winAlpha * 0.9f), kElemRound);

                const char* name = LootDatabase::CategoryName(i);
                const ImVec2 nameDim = rowFont->CalcTextSizeA(rowFontSize, FLT_MAX, 0.f, name);
                const ImVec2 namePos(
                    rr.Min.x + 12.f,
                    rr.Min.y + (kCategoryRowH - nameDim.y) * 0.5f);
                dl->AddText(rowFont, rowFontSize, namePos,
                    UiTextColor(selected, rowHovered, winAlpha), name);

                ImGui::PopID();
            }

            ImGui::EndChild();
        }
        ImGui::End();

        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(7);
    }

    static void DrawCategoryPopup(int* category, int catCount, float alpha) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kElemRound);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 6.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, kElemRound);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, kAshBg);
        ImGui::PushStyleColor(ImGuiCol_Border, AshU32(kAshStroke, alpha));

        ImGui::SetNextWindowPos(ImVec2(s_categoryTriggerRect.Min.x, s_categoryTriggerRect.Max.y + 2.f));
        ImGui::SetNextWindowSize(ImVec2(s_categoryTriggerRect.GetWidth(), 0.f));

        if (ImGui::BeginPopup("##loot_cat_drop")) {
            s_categorySideRect = ImGui::GetCurrentWindow()->Rect();

            ImFont* rowFont = ItemFont();
            const float rowFontSize = rowFont->FontSize;

            for (int i = 0; i < catCount; ++i) {
                ImGui::PushID(i);
                const bool selected = (*category == i);
                ImGui::InvisibleButton("##cat_row", ImVec2(-1.f, kCategoryRowH));
                const bool rowHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    *category = i;
                    ImGui::CloseCurrentPopup();
                }

                const ImRect rr(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                ImDrawList* dl = ImGui::GetWindowDrawList();

                if (selected) {
                    dl->AddRectFilled(rr.Min, rr.Max, AshU32(kAshSelect, alpha), kElemRound);
                    dl->AddRectFilled(rr.Min, ImVec2(rr.Min.x + 3.f, rr.Max.y), AccentU32(alpha * 0.85f), 2.f);
                }
                else if (rowHovered)
                    dl->AddRectFilled(rr.Min, rr.Max, AshU32(kAshHover, alpha * 0.9f), kElemRound);

                const char* name = LootDatabase::CategoryName(i);
                const ImVec2 nameDim = rowFont->CalcTextSizeA(rowFontSize, FLT_MAX, 0.f, name);
                const ImVec2 namePos(
                    rr.Min.x + 12.f,
                    rr.Min.y + (kCategoryRowH - nameDim.y) * 0.5f);
                dl->AddText(rowFont, rowFontSize, namePos,
                    UiTextColor(selected, rowHovered, alpha), name);

                ImGui::PopID();
            }

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }

    static void HandleCategoryOutsideClick() {
        if (!s_categoryOpen || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;
        const ImVec2 mp = ImGui::GetIO().MousePos;
        if (s_categoryTriggerRect.Contains(mp) || s_categorySideRect.Contains(mp))
            return;
        s_categoryOpen = false;
    }

    static void DrawLootRow(const LootCatalogItem& item, bool* enabled, float panelAlpha) {
        ImGui::PushID(static_cast<int>(item.id));

        ImGui::Dummy(ImVec2(-1.f, kRowH));
        const ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

        constexpr float kCheckPad = 10.f;
        const float checkX = r.Max.x - kCheckSz - kCheckPad;
        const ImRect checkRect(
            ImVec2(checkX, r.Min.y + (kRowH - kCheckSz) * 0.5f),
            ImVec2(checkX + kCheckSz, r.Min.y + (kRowH + kCheckSz) * 0.5f));

        const float labelW = checkRect.Min.x - r.Min.x - 8.f;
        ImGui::SetCursorScreenPos(r.Min);
        ImGui::InvisibleButton("##loot_row_label", ImVec2(labelW, kRowH));
        const bool labelHovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            *enabled = !*enabled;
            RowAnim(item.id).press = 1.f;
        }

        ImGui::SetCursorScreenPos(checkRect.Min);
        ImGui::InvisibleButton("##loot_row_check", checkRect.GetSize());
        const bool checkHovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            *enabled = !*enabled;
            RowAnim(item.id).press = 1.f;
        }

        const bool hovered = labelHovered || checkHovered;

        RowAnimState& anim = RowAnim(item.id);
        const float targetOn = *enabled ? 1.f : 0.f;
        anim.enabled = AnimStep(anim.enabled, targetOn, 14.f);
        anim.hover = AnimStep(anim.hover, hovered ? 1.f : 0.f, 16.f);
        anim.check = AnimStep(anim.check, targetOn, 18.f);
        anim.press = AnimStep(anim.press, 0.f, 10.f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec4 itemColor = LootDatabase::GetColor(item.id);

        const float bgMix = (anim.enabled * 0.5f) + (anim.hover * 0.32f);
        if (bgMix > 0.01f) {
            const ImU32 rowBg = LerpColor(
                AshU32(kAshHover, panelAlpha * (0.20f + 0.35f * bgMix)),
                AshU32(itemColor, panelAlpha * 0.12f * anim.enabled),
                anim.enabled * 0.65f);
            dl->AddRectFilled(r.Min, r.Max, rowBg, 5.f);
        }

        if (anim.enabled > 0.04f) {
            dl->AddRectFilled(
                ImVec2(r.Min.x + 4.f, r.Min.y + 5.f),
                ImVec2(r.Min.x + 7.f, r.Max.y - 5.f),
                AshU32(itemColor, panelAlpha * anim.enabled), 2.f);
        }

        dl->AddLine(
            ImVec2(r.Min.x + 10.f, r.Max.y - 0.5f),
            ImVec2(r.Max.x - 6.f, r.Max.y - 0.5f),
            LerpColor(AshU32(kAshStroke, 0.18f), AccentU32(0.28f), anim.hover * 0.5f),
            1.f);

        const ImU32 nameCol = ItemRowTextColor(itemColor, anim.enabled, hovered, panelAlpha);

        const float textLift = anim.press * 1.2f;
        ImFont* rowFont = ItemFont();
        if (!rowFont) {
            ImGui::PopID();
            return;
        }
        const float rowFontSize = rowFont->FontSize;
        const ImVec2 nameDim = rowFont->CalcTextSizeA(rowFontSize, FLT_MAX, 0.f, item.name.c_str());
        const ImVec2 textPos(
            r.Min.x + 12.f + anim.press * 0.4f,
            r.Min.y + (kRowH - nameDim.y) * 0.5f - textLift);
        dl->AddText(rowFont, rowFontSize, textPos, nameCol, item.name.c_str());

        DrawFillCheckbox(dl, checkRect, anim.check, anim.hover, panelAlpha);

        ImGui::PopID();
    }

    static void DrawFilterHint(float alpha) {
        const int globalOn = g_Globals.Loot.EnabledFilterCount();
        if (globalOn > 0)
            return;

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowMax(rowMin.x + width, rowMin.y + kHintH);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(rowMin, rowMax, AshU32(kAshFrame, alpha * 0.9f), kElemRound);
        dl->AddRect(rowMin, rowMax, AccentU32(alpha * 0.45f), kElemRound, 0, kBorderThick);

        const char* hint = "Check items below to show in-game";
        ImFont* f = SafeFont(font::small_font);
        if (!f)
            return;
        const float fs = f->FontSize;
        const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.f, hint);
        const ImVec2 tp(
            rowMin.x + (width - ts.x) * 0.5f,
            rowMin.y + (kHintH - ts.y) * 0.5f);
        dl->AddText(f, fs, tp, AccentU32(alpha * 0.85f), hint);

        ImGui::Dummy(ImVec2(-1.f, kHintH + 4.f));
    }

    static void DrawLootItemsHeader(ImDrawList* dl, int categoryOn, int categoryTotal, int globalOn, float alpha) {
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowMax(rowMin.x + width, rowMin.y + kHeaderH);

        dl->AddLine(rowMin, ImVec2(rowMax.x, rowMin.y), AshU32(kAshStroke, 0.55f * alpha), 1.f);
        dl->AddLine(
            ImVec2(rowMin.x, rowMax.y - 1.f),
            rowMax,
            LerpColor(AshU32(kAshStroke, 0.40f * alpha), AccentU32(alpha * 0.5f), globalOn > 0 ? 0.6f : 0.f),
            1.5f);
        dl->AddRectFilled(
            ImVec2(rowMin.x, rowMin.y + 1.f),
            ImVec2(rowMin.x + 28.f, rowMin.y + 2.5f),
            AccentU32(alpha * 0.7f), 1.f);

        const char* title = "Loot Items";
        ImFont* titleFont = SafeFont(font::inter_medium);
        const ImVec2 titleDim = titleFont->CalcTextSizeA(kTitleSize, FLT_MAX, 0.f, title);

        char counter[56];
        if (globalOn > 0)
            snprintf(counter, sizeof(counter), "%d in-game · %d/%d", globalOn, categoryOn, categoryTotal);
        else
            snprintf(counter, sizeof(counter), "%d/%d", categoryOn, categoryTotal);

        ImFont* counterFont = SafeFont(font::small_font);
        const float counterSize = counterFont->FontSize * 0.92f;
        const ImVec2 counterDim = counterFont->CalcTextSizeA(counterSize, FLT_MAX, 0.f, counter);

        const float titleX = rowMin.x + 2.f;
        const float titleY = rowMin.y + (kHeaderH - titleDim.y) * 0.5f;
        const float counterX = rowMax.x - counterDim.x;
        const float counterY = rowMin.y + (kHeaderH - counterDim.y) * 0.5f;

        const ImU32 titleCol = ThemeU32(c::text::label::active, alpha);
        const ImU32 counterCol = LerpColor(
            ThemeU32(c::text::description::regular, alpha),
            AccentU32(alpha),
            globalOn > 0 ? ImMax(s_counterAnim, 0.35f) : 0.f);

        dl->AddText(titleFont, kTitleSize, ImVec2(titleX, titleY), titleCol, title);
        dl->AddText(counterFont, counterSize, ImVec2(counterX, counterY), counterCol, counter);

        ImGui::Dummy(ImVec2(-1.f, kHeaderH));
    }

    static void PushPickerStyle(float panelAlpha) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, panelAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 6.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kElemRound);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kElemRound);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);

        ImGui::PushStyleColor(ImGuiCol_Text, utils::ImColorToImVec4(c::text::label::active));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, kAshFrame);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kAshHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kAshSelect);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, kAshBg);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.08f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.f, 0.f, 0.f, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.28f, 0.28f, 0.30f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.36f, 0.36f, 0.38f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.42f, 0.42f, 0.44f, 0.65f));
    }

    static void PopPickerStyle() {
        ImGui::PopStyleColor(10);
        ImGui::PopStyleVar(6);
    }

    static void DrawPickerContent(float panelAlpha, int catCount) {
        ImDrawList* wdl = ImGui::GetWindowDrawList();

        DrawFieldLabel("Search", panelAlpha);

        ImGui::PushStyleColor(ImGuiCol_Text, utils::ImColorToImVec4(c::text::label::active));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, utils::ImColorToImVec4(c::text::description::regular));
        ImGui::InputTextWithHint(
            "##loot_search_input",
            "Search items...",
            s_search,
            sizeof(s_search));
        ImGui::PopStyleColor(2);

        {
            const bool searchActive = ImGui::IsItemActive() || ImGui::IsItemHovered();
            const ImRect searchRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            const ImU32 searchBorder = searchActive
                ? AccentU32(panelAlpha * 0.85f)
                : AshU32(kAshStroke, panelAlpha * 0.65f);
            wdl->AddRect(searchRect.Min, searchRect.Max, searchBorder, kElemRound, 0, kBorderThick);
        }

        if (ImGui::IsItemActive())
            ImGui::GetIO().WantCaptureKeyboard = true;

        if (!s_embeddedMode)
            s_categoryOpenAnim = AnimStep(s_categoryOpenAnim, s_categoryOpen ? 1.f : 0.f, 14.f);
        DrawCategoryTrigger(s_category, catCount, panelAlpha);

        if (s_embeddedMode)
            DrawCategoryPopup(&s_category, catCount, panelAlpha);

        const float btnW = (ImGui::GetContentRegionAvail().x - 5.f) * 0.5f;
        if (DrawAshButton("select_all", "Select All", ImVec2(btnW, kBtnH), panelAlpha, true)) {
            std::vector<const LootCatalogItem*> items;
            CollectVisible(items);
            for (const LootCatalogItem* item : items)
                g_Globals.Loot.SetItemFilter(item->id, true);
        }
        ImGui::SameLine(0.f, 5.f);
        if (DrawAshButton("clear_all", "Clear All", ImVec2(btnW, kBtnH), panelAlpha)) {
            std::vector<const LootCatalogItem*> items;
            CollectVisible(items);
            for (const LootCatalogItem* item : items)
                g_Globals.Loot.SetItemFilter(item->id, false);
        }

        std::vector<const LootCatalogItem*> visible;
        CollectVisible(visible);
        PruneRowAnim(visible);

        int enabled = 0;
        for (const LootCatalogItem* item : visible) {
            if (g_Globals.Loot.IsItemFiltered(item->id))
                ++enabled;
        }

        const int globalOn = g_Globals.Loot.EnabledFilterCount();
        const float counterTarget = visible.empty() ? 0.f : (float)enabled / (float)visible.size();
        s_counterAnim = AnimStep(s_counterAnim, counterTarget, 10.f);

        DrawFilterHint(panelAlpha);
        DrawLootItemsHeader(wdl, enabled, static_cast<int>(visible.size()), globalOn, panelAlpha);

        const float availY = ImGui::GetContentRegionAvail().y;
        const float listHeight = ImMax(48.f, availY > 2.f ? availY - 2.f : 48.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 4.f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.28f, 0.28f, 0.30f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.36f, 0.36f, 0.38f, 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.42f, 0.42f, 0.44f, 0.65f));
        ImGui::BeginChild("##loot_items_list", ImVec2(-1.f, listHeight), false,
            ImGuiWindowFlags_AlwaysVerticalScrollbar);

        {
            ImDrawList* listDl = ImGui::GetWindowDrawList();
            const ImVec2 cmin = ImGui::GetWindowPos();
            const ImVec2 cmax = cmin + ImGui::GetWindowSize();
            listDl->AddRect(cmin, cmax, AshU32(kAshStroke, panelAlpha * 0.35f), kElemRound, 0, 1.f);
        }

        if (visible.empty()) {
            ImGui::SetCursorPos(ImVec2(12.f, 12.f));
            ImFont* hintFont = SafeFont(font::small_font);
            if (hintFont)
                ImGui::PushFont(hintFont);
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, panelAlpha),
                s_search[0] ? "No matches." : "No items in this category.");
            if (hintFont)
                ImGui::PopFont();
        }
        else {
            for (size_t i = 0; i < visible.size(); ++i) {
                const LootCatalogItem* item = visible[i];
                if (!item)
                    continue;
                bool on = g_Globals.Loot.IsItemFiltered(item->id);
                DrawLootRow(*item, &on, panelAlpha);
                g_Globals.Loot.SetItemFilter(item->id, on);
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
}

namespace LootUI {

void DrawPickerEmbedded() {
    if (!ImGui::GetCurrentWindow())
        return;

    s_embeddedMode = true;
    s_panelAlpha = 1.f;
    s_categoryOpen = false;

    const int catCount = LootDatabase::CategoryCount();
    if (catCount <= 0) {
        ImGui::TextDisabled("Loot catalog unavailable.");
        s_embeddedMode = false;
        return;
    }
    if (s_category < 0 || s_category >= catCount)
        s_category = 0;

    s_panelPos = ImGui::GetWindowPos();
    s_panelSize = ImGui::GetWindowSize();

    PushPickerStyle(1.f);
    DrawPickerContent(1.f, catCount);
    PopPickerStyle();

    s_embeddedMode = false;
}

void DrawPickerPanel(bool visualTabActive) {
    if (!visualTabActive || !g_Globals.Loot.Enabled || !g_Globals.Loot.ShowPicker) {
        s_panelAlpha = AnimStep(s_panelAlpha, 0.f, 8.f);
        s_categoryOpen = false;
        return;
    }

    ImGuiWindow* menu = ImGui::FindWindowByName("imgui menu");
    if (!menu)
        menu = ImGui::FindWindowByName("ImGui Menu Insignia");
    if (!menu)
        return;

    s_panelAlpha = AnimStep(s_panelAlpha, 1.f, 7.f);

    const int catCount = LootDatabase::CategoryCount();
    if (catCount <= 0)
        return;
    if (s_category < 0 || s_category >= catCount)
        s_category = 0;

    const ImVec2 panelSize(kPanelW, ImMax(420.f, menu->SizeFull.y - 44.f));
    const ImVec2 panelPos(menu->Pos.x + menu->SizeFull.x + 8.f, menu->Pos.y + 36.f);
    s_panelPos = panelPos;
    s_panelSize = panelSize;

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);

    const float panelAlpha = PanelAlpha();
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, panelAlpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kPanelRound);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("Loot ESP Picker", nullptr, flags)) {
        ImDrawList* wdl = ImGui::GetWindowDrawList();
        DrawPanelChrome(wdl, ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(), panelAlpha);
        PushPickerStyle(panelAlpha);
        DrawPickerContent(panelAlpha, catCount);
        PopPickerStyle();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);

    if (!s_embeddedMode) {
        DrawCategorySidePanel(&s_category, catCount, panelAlpha);
        HandleCategoryOutsideClick();
    }
}

}
