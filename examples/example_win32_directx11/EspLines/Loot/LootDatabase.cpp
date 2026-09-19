#include "LootDatabase.hpp"

#include <algorithm>

namespace {
    static std::unordered_map<uint32_t, std::string> s_names;
    static std::vector<std::vector<uint32_t>> s_categories;
    static std::vector<LootCatalogItem> s_catalog;
    static bool s_built = false;

    static const char* kCategoryNames[] = {
        "Rifles",
        "Marksman Rifles",
        "Machine Guns",
        "Sub Machine Guns",
        "Shotguns",
        "Sniper Rifles",
        "Pistols",
        "Melee",
        "Launchers & Crossbow",
        "Ammo",
        "Healing Utilities",
        "Projectiles & Landmines",
        "Armors & Backpacks",
        "Attachments",
        "Miscellaneous"
    };

    static void AddItem(uint32_t id, const char* name) {
        s_names[id] = name;
    }

    static void BuildNames() {
        AddItem(2, "M4A1"); AddItem(80, "M4A1-I"); AddItem(81, "M4A1-II"); AddItem(82, "M4A1-III");
        AddItem(6, "AK47"); AddItem(11, "M14"); AddItem(63, "M14-I"); AddItem(126, "M14-II"); AddItem(127, "M14-III");
        AddItem(12, "SCAR"); AddItem(178, "SCAR-I"); AddItem(179, "SCAR-II"); AddItem(180, "SCAR-III");
        AddItem(14, "GROZA"); AddItem(70, "GROZA-X"); AddItem(24, "FAMAS"); AddItem(67, "FAMAS-I");
        AddItem(130, "FAMAS-II"); AddItem(131, "FAMAS-III"); AddItem(28, "XM8"); AddItem(33, "AN94");
        AddItem(39, "Plasma"); AddItem(46, "AUG"); AddItem(193, "AUG-I"); AddItem(194, "AUG-II"); AddItem(195, "AUG-III");
        AddItem(47, "PARAFAL"); AddItem(57, "KingFisher"); AddItem(73, "G36-ASSAULT"); AddItem(74, "G36-RANGE");
        AddItem(18, "SKS"); AddItem(26, "SVD"); AddItem(72, "SVD-Y"); AddItem(48, "Woodpecker"); AddItem(89, "AC80");
        AddItem(19, "M249"); AddItem(71, "M249-X"); AddItem(30, "M60"); AddItem(61, "M60-I");
        AddItem(122, "M60-II"); AddItem(123, "M60-III"); AddItem(54, "Kord");
        AddItem(7, "UMP"); AddItem(8, "MP5"); AddItem(60, "MP5-I"); AddItem(120, "MP5-II"); AddItem(121, "MP5-III");
        AddItem(13, "VSS"); AddItem(62, "VSS-I"); AddItem(124, "VSS-II"); AddItem(125, "VSS-III");
        AddItem(15, "MP40"); AddItem(32, "P90"); AddItem(35, "CG15"); AddItem(43, "THOMPSON");
        AddItem(49, "Vector"); AddItem(69, "DOUBLE-VECTOR"); AddItem(88, "MAC10");
        AddItem(228, "MAC10-I"); AddItem(229, "MAC10-II"); AddItem(230, "MAC10-III"); AddItem(150, "BIZON");
        AddItem(5, "M1014"); AddItem(184, "M1014-I"); AddItem(185, "M1014-II"); AddItem(186, "M1014-III");
        AddItem(29, "SPAS12"); AddItem(41, "M1887"); AddItem(119, "M1887-X"); AddItem(50, "MAG-7");
        AddItem(86, "Charge Buster"); AddItem(181, "Trogon-Shotgun"); AddItem(182, "Trogon-Grenade");
        AddItem(4, "AWM"); AddItem(65, "AWM-Y"); AddItem(21, "Kar98K"); AddItem(64, "Kar98K-I");
        AddItem(128, "Kar98K-II"); AddItem(129, "Kar98K-III"); AddItem(45, "M82B"); AddItem(75, "M24");
        AddItem(78, "Heal Sniper"); AddItem(197, "VSK94");
        AddItem(3, "USP"); AddItem(56, "USP-2"); AddItem(9, "Desert Eagle"); AddItem(10, "G18");
        AddItem(20, "M1873"); AddItem(25, "M500"); AddItem(55, "M1917"); AddItem(58, "MINI UZI");
        AddItem(34, "Katana"); AddItem(16, "PAN"); AddItem(17, "MACHETE"); AddItem(27, "Bat");
        AddItem(51, "Scythe"); AddItem(53, "FF Knife"); AddItem(1, "Fist");
        AddItem(601, "Grenade"); AddItem(603, "Smoke Grenade"); AddItem(1201, "Gloo Wall");
        AddItem(1204, "Gloo Wall"); AddItem(602, "Flash Grenade"); AddItem(608, "Freeze Bomb");
        AddItem(21002, "M590"); AddItem(23, "M79"); AddItem(36, "RGS-50"); AddItem(196, "FGL-24");
        AddItem(100, "Flamethrower"); AddItem(99, "Shield Gun"); AddItem(21001, "Heal Pistol");
        AddItem(21006, "Winchester"); AddItem(638, "Thompson-x");
        AddItem(501, "Muzzle LV1"); AddItem(502, "Muzzle LV2"); AddItem(503, "Muzzle LV3"); AddItem(504, "Silencer");
        AddItem(213, "Inhaler"); AddItem(221, "Energizer"); AddItem(3001, "FF Coin");
        AddItem(106, "Repair Kit"); AddItem(201, "AR Ammo"); AddItem(203, "Sniper Ammo");
        AddItem(204, "SG Ammo"); AddItem(205, "SMG Ammo"); AddItem(207, "Launcher Ammo");
        AddItem(102, "Medikit"); AddItem(1205, "Super Medikit");
        AddItem(401, "Backpack LV1"); AddItem(402, "Backpack LV2"); AddItem(403, "Backpack LV3");
        AddItem(301, "Vest LV1"); AddItem(302, "Vest LV2"); AddItem(303, "Vest LV3"); AddItem(309, "Vest LV4");
        AddItem(304, "Helmet LV1"); AddItem(305, "Helmet LV2"); AddItem(306, "Helmet LV3");
        AddItem(534, "Scope 2x"); AddItem(535, "Scope 4x"); AddItem(537, "Thermal Scope");
        AddItem(511, "Magazine LV1"); AddItem(512, "Magazine LV2"); AddItem(513, "Magazine LV3");
        AddItem(518, "Fast Magazine"); AddItem(521, "Foregrip LV1"); AddItem(522, "Foregrip LV2");
        AddItem(523, "Foregrip LV3"); AddItem(526, "Light Foregrip");
        AddItem(541, "Stock LV1"); AddItem(542, "Stock LV2"); AddItem(543, "Stock LV3"); AddItem(547, "Power Stock");
        AddItem(3008, "Upgrade Chip"); AddItem(5601, "Shield Booster"); AddItem(1401, "Landmine");
        AddItem(617, "Gloo Melter"); AddItem(624, "Dragon Freeze"); AddItem(5605, "Helmet Thickener");
        AddItem(5652, "Gloo Catalyst"); AddItem(50010, "Arsenal Key");
        AddItem(91007, "Skill: Chrono"); AddItem(91010, "Skill: Dmitri"); AddItem(91011, "Skill: Kenta");
        AddItem(91012, "Skill: Homer"); AddItem(91053, "Skill: Ignis"); AddItem(91002, "Skill: AI24");
        AddItem(93002, "Supply Box"); AddItem(93007, "Mini Turret"); AddItem(10006, "M.Truck Drop");
        AddItem(92101, "UAV Lite"); AddItem(92102, "Heal UAV Lite");
        AddItem(701, "Mushroom lv1"); AddItem(702, "Mushroom lv2"); AddItem(703, "Mushroom lv3"); AddItem(707, "Mushroom lv4");
    }

    static void BuildCategories() {
        s_categories = {
            { 2, 80, 81, 82, 6, 11, 63, 126, 127, 12, 178, 179, 180, 14, 70, 24, 67, 130, 131, 28, 33, 39, 46, 193, 194, 195, 47, 57, 73, 74 },
            { 18, 26, 72, 48, 89 },
            { 19, 71, 30, 61, 122, 123, 54 },
            { 7, 8, 60, 120, 121, 13, 62, 124, 125, 15, 32, 35, 43, 49, 69, 88, 228, 229, 230, 150 },
            { 5, 184, 185, 186, 29, 41, 119, 50, 86, 181, 182, 21002 },
            { 4, 65, 21, 64, 128, 129, 45, 75, 78, 197 },
            { 3, 56, 9, 10, 20, 25, 55, 58, 21001 },
            { 34, 16, 17, 27, 51, 53, 1 },
            { 23, 36, 196, 100, 99, 21006, 638 },
            { 201, 203, 204, 205, 207 },
            { 102, 1205, 213, 221, 5601 },
            { 601, 603, 1201, 1204, 602, 608, 1401, 617, 624 },
            { 301, 302, 303, 309, 304, 305, 306, 5605, 401, 402, 403 },
            { 501, 502, 503, 504, 534, 535, 537, 511, 512, 513, 518, 521, 522, 523, 526, 541, 542, 543, 547, 3008, 5652 },
            { 3001, 106, 50010, 701, 702, 703, 707, 91007, 91010, 91011, 91012, 91053, 91002, 93002, 93007, 10006, 92101, 92102 }
        };
    }

    static void BuildCatalog() {
        s_catalog.clear();
        for (int c = 0; c < static_cast<int>(s_categories.size()); ++c) {
            for (uint32_t id : s_categories[c]) {
                LootCatalogItem item;
                item.id = id;
                item.category = c;
                auto it = s_names.find(id);
                item.name = it != s_names.end() ? it->second : ("Item " + std::to_string(id));
                s_catalog.push_back(std::move(item));
            }
        }
        std::sort(s_catalog.begin(), s_catalog.end(), [](const LootCatalogItem& a, const LootCatalogItem& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.name < b.name;
        });
    }

    static void EnsureBuilt() {
        if (s_built) return;
        BuildNames();
        BuildCategories();
        BuildCatalog();
        s_built = true;
    }
}

namespace LootDatabase {

uint32_t NormalizeItemId(uint32_t rawItemId) {
    if (rawItemId == 0)
        return 0;

    int id = static_cast<int>(rawItemId);
    if (id < 0)
        id += 25000;

    // BR packed ids: low 16 bits = catalog id (0x00020004 -> AWM 4, 0x00020041 -> AWM-Y 65).
    if (id > 0xFFFF) {
        const int low = id & 0xFFFF;
        if (low > 0 && low < 500)
            return static_cast<uint32_t>(low);
    }

    // BR ground loot encoding (some builds): 20004 -> AWM, 20065 -> AWM-Y
    if (id >= 20000 && id < 21000) {
        const int sub = id - 20000;
        if (sub > 0)
            return static_cast<uint32_t>(sub);
    }

    // CS / classic pickup encoding: 21004 -> AWM, 21065 -> AWM-Y
    if (id >= 21000 && id < 22000) {
        const int sub = id - 21000;
        if (sub == 1) return 21001u;
        if (sub == 2) return 21002u;
        if (sub == 6) return 21006u;
        if (sub == 20) return 1u;
        if (sub > 0)
            return static_cast<uint32_t>(sub);
    }

    return static_cast<uint32_t>(id);
}

int CategoryCount() {
    EnsureBuilt();
    return static_cast<int>(s_categories.size());
}

const char* CategoryName(int index) {
    EnsureBuilt();
    if (index < 0 || index >= static_cast<int>(sizeof(kCategoryNames) / sizeof(kCategoryNames[0])))
        return "Unknown";
    return kCategoryNames[index];
}

const char* const* CategoryNameList() {
    return kCategoryNames;
}

const std::unordered_map<uint32_t, std::string>& ItemNames() {
    EnsureBuilt();
    return s_names;
}

const std::vector<std::vector<uint32_t>>& Categories() {
    EnsureBuilt();
    return s_categories;
}

const std::vector<LootCatalogItem>& Catalog() {
    EnsureBuilt();
    return s_catalog;
}

std::string GetName(uint32_t itemId) {
    EnsureBuilt();
    itemId = NormalizeItemId(itemId);
    const auto it = s_names.find(itemId);
    if (it != s_names.end())
        return it->second;
    return "Item " + std::to_string(itemId);
}

int GetCategoryIndex(uint32_t itemId) {
    EnsureBuilt();
    itemId = NormalizeItemId(itemId);
    for (int i = 0; i < static_cast<int>(s_categories.size()); ++i) {
        const auto& ids = s_categories[i];
        if (std::find(ids.begin(), ids.end(), itemId) != ids.end())
            return i;
    }
    return -1;
}

ImVec4 GetColor(uint32_t itemId) {
    itemId = NormalizeItemId(itemId);
    if (itemId == 309 || itemId == 1205 || itemId == 5601 || itemId == 80 || itemId == 81 || itemId == 82 ||
        itemId == 127 || itemId == 180 || itemId == 70)
        return ImVec4(1.f, 0.85f, 0.2f, 1.f);
    if (itemId == 303 || itemId == 306 || itemId == 403 || itemId == 503 || itemId == 513 ||
        itemId == 523 || itemId == 543 || itemId == 703)
        return ImVec4(0.81f, 0.38f, 0.94f, 1.f);
    if (itemId == 302 || itemId == 305 || itemId == 402 || itemId == 502 || itemId == 512 ||
        itemId == 522 || itemId == 542 || itemId == 702)
        return ImVec4(0.18f, 0.65f, 0.94f, 1.f);
    if (itemId == 301 || itemId == 304 || itemId == 401 || itemId == 501 || itemId == 511 ||
        itemId == 521 || itemId == 541 || itemId == 701 || itemId == 102)
        return ImVec4(0.36f, 0.9f, 0.46f, 1.f);
    if (itemId == 201 || itemId == 203 || itemId == 204 || itemId == 205 || itemId == 207)
        return ImVec4(0.8f, 0.8f, 0.8f, 1.f);
    return ImVec4(0.95f, 0.95f, 0.95f, 1.f);
}

}
