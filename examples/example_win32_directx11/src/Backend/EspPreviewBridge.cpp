#include <examples/example_win32_directx11/src/Backend/BackendBridge.hpp>
#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Visuals/Namegun.h>
#include <examples/example_win32_directx11/esp.h>

void Backend_SyncEspPreview()
{
    Namegun::Init();

    static constexpr short kPreviewGunId = 12; // SCAR — sample weapon for preview

    const std::string icon = Namegun::GetGunIcon(kPreviewGunId);
    m_esp_draw.m_items[1].text = icon.empty() ? std::string("\ue078") : icon;

    const std::string gunName = Namegun::GetGunName(kPreviewGunId);
    m_esp_draw.m_items[7].text = gunName.empty() ? std::string("SCAR") : gunName;

    const ImVec4& nameCol = g_Globals.Visuals.NameColor.Value;
    const ImVec4& distCol = g_Globals.Visuals.DistanceColor.Value;
    const ImVec4& weaponCol = g_Globals.Visuals.WeaponColor.Value;

    m_esp_draw.m_items[1].col = ImColor(weaponCol.x, weaponCol.y, weaponCol.z, weaponCol.w);
    m_esp_draw.m_items[2].col = ImColor(nameCol.x, nameCol.y, nameCol.z, nameCol.w);
    m_esp_draw.m_items[3].col = ImColor(distCol.x, distCol.y, distCol.z, distCol.w);
    m_esp_draw.m_items[7].col = ImColor(weaponCol.x, weaponCol.y, weaponCol.z, weaponCol.w);
}
