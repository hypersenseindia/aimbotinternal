#include "LootScanner.hpp"

#include <vector>
#include <algorithm>

#include <examples/example_win32_directx11/src/Globals.hpp>
#include <examples/example_win32_directx11/EspLines/Offsets.hpp>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>
#include <examples/example_win32_directx11/EspLines/Math/TMatrix.hpp>
#include <examples/example_win32_directx11/EspLines/Loot/LootTypes.hpp>
#include <examples/example_win32_directx11/EspLines/Loot/LootDatabase.hpp>

namespace {
    constexpr int kMaxLootPerScan = 450;
    constexpr uint32_t kDictEntrySize = 0x10;

    uint32_t ReadIdAtOffset(uint32_t base, uintptr_t offset, bool asShort) {
        if (base == 0)
            return 0;

        if (asShort) {
            short s = 0;
            if (!Mem.Read(base + offset, s) || s == 0)
                return 0;
            int v = s;
            if (v < 0)
                v += 25000;
            return LootDatabase::NormalizeItemId(static_cast<uint32_t>(v));
        }

        uint32_t v = 0;
        if (!Mem.Read(base + offset, v) || v == 0)
            return 0;
        return LootDatabase::NormalizeItemId(v);
    }

    bool ReadPickupItemId(uint32_t baseObj, uint32_t& itemId) {
        itemId = 0;

        uint32_t pickupList = 0;
        if (!Mem.Read(baseObj + Offsets::LootPickupList, pickupList) || !pickupList) {
            if (!Mem.Read(baseObj + Offsets::LootPickupListAlt, pickupList) || !pickupList)
                return false;
        }

        uint32_t pickupItemsArray = 0;
        int pickupCount = 0;
        if (!Mem.Read(pickupList + 0x08, pickupItemsArray) || !pickupItemsArray)
            return false;
        if (!Mem.Read(pickupList + 0x0C, pickupCount) || pickupCount <= 0)
            return false;

        pickupItemsArray += 0x10;
        uint32_t pickupItem = 0;
        if (!Mem.Read(pickupItemsArray, pickupItem) || !pickupItem)
            return false;

        // Try multiple layouts — BR drops often use +0x24 or packed uint32 (0x00020004 = AWM).
        static const uintptr_t kPickupOffsets[] = { 0x10, 0x18, 0x24, 0x14, 0x1C, 0x20, 0x28, 0x0C };
        for (uintptr_t off : kPickupOffsets) {
            itemId = ReadIdAtOffset(pickupItem, off, true);
            if (itemId != 0)
                break;
            itemId = ReadIdAtOffset(pickupItem, off, false);
            if (itemId != 0)
                break;
        }

        if (itemId == 0) {
            static const uintptr_t kBaseObjOffsets[] = { 0x10, 0x18, 0x24, 0x14, 0x1C, 0x20 };
            for (uintptr_t off : kBaseObjOffsets) {
                itemId = ReadIdAtOffset(baseObj, off, true);
                if (itemId != 0)
                    break;
                itemId = ReadIdAtOffset(baseObj, off, false);
                if (itemId != 0)
                    break;
            }
        }

        if (itemId == 0) {
            const uint32_t weaponObj = Mem.ReadS<uint32_t>(pickupItem + Offsets::Weapon);
            if (weaponObj != 0) {
                uint32_t wId = Mem.ReadS<uint32_t>(weaponObj + Offsets::Weapon_ItemId);
                itemId = LootDatabase::NormalizeItemId(wId);
                if (itemId == 0) {
                    const uint32_t weaponData = Mem.ReadS<uint32_t>(weaponObj + Offsets::WeaponData);
                    if (weaponData != 0) {
                        static const uintptr_t kDataOffs[] = { 0x10, 0x14, 0x18, 0x1C };
                        for (uintptr_t off : kDataOffs) {
                            wId = Mem.ReadS<uint32_t>(weaponData + off);
                            itemId = LootDatabase::NormalizeItemId(wId);
                            if (itemId != 0)
                                break;
                        }
                    }
                }
            }
        }

        return itemId != 0;
    }

    bool ReadObjectPosition(uint32_t baseObj, Vector3& pos) {
        uint32_t lTransform = 0;
        if (!Mem.Read(baseObj + Offsets::LootTransformComponent, lTransform) || !lTransform)
            return false;

        if (TransformUtils::GetPosition(lTransform, pos) && pos != Vector3::Zero())
            return true;

        return Mem.Read(lTransform + 0x3C, pos) && pos != Vector3::Zero();
    }

    void ScanObjectDict(uint32_t objDict, const Vector3& cameraPos, float maxDist,
        std::vector<GroundLootEntry>& out) {
        if (!objDict || out.size() >= static_cast<size_t>(kMaxLootPerScan))
            return;

        int objCount = 0;
        if (!Mem.Read(objDict + 0x10, objCount) || objCount <= 0 || objCount > 800)
            return;

        uint32_t objEntries = 0;
        if (!Mem.Read(objDict + 0x0C, objEntries) || !objEntries)
            return;

        objEntries += 0x10;

        for (int j = 0; j < objCount && out.size() < static_cast<size_t>(kMaxLootPerScan); ++j) {
            const uint32_t objEntryPtr = objEntries + static_cast<uint32_t>(j) * kDictEntrySize;

            int objHash = 0;
            if (!Mem.Read(objEntryPtr + 0x0, objHash) || objHash < 0)
                continue;

            uint32_t baseObj = 0;
            if (!Mem.Read(objEntryPtr + 0x0C, baseObj) || !baseObj)
                continue;

            uint32_t itemId = 0;
            if (!ReadPickupItemId(baseObj, itemId))
                continue;

            Vector3 pos{};
            if (!ReadObjectPosition(baseObj, pos))
                continue;

            const float dist = Vector3::Distance(cameraPos, pos);
            if (dist > maxDist || dist < 0.05f)
                continue;

            GroundLootEntry entry;
            entry.ItemId = itemId;
            entry.Position = pos;
            entry.Distance = dist;
            entry.Name = LootDatabase::GetName(itemId);
            out.push_back(std::move(entry));
        }
    }
}

namespace FWork {
namespace Loot {

void ScanGround(uint32_t currentGame, const Vector3& cameraPos) {
    if (!g_Globals.Loot.Enabled || !currentGame || !Mem.IsReady()) {
        g_Globals.Loot.GroundLoot.clear();
        return;
    }

    try {
    std::vector<GroundLootEntry> found;
    found.reserve(128);

    const float maxDist = static_cast<float>(std::clamp(g_Globals.Loot.RenderDistance, 20, 500));

    auto scanLom = [&](uint32_t lom) {
        if (!lom)
            return;

        uint32_t objectDict = 0;
        if (!Mem.Read(lom + 0x0C, objectDict) || !objectDict)
            return;

        int containerCount = 0;
        if (!Mem.Read(objectDict + 0x10, containerCount) || containerCount <= 0 || containerCount > 200)
            return;

        uint32_t containerEntries = 0;
        if (!Mem.Read(objectDict + 0x0C, containerEntries) || !containerEntries)
            return;

        containerEntries += 0x10;

        for (int i = 0; i < containerCount && found.size() < static_cast<size_t>(kMaxLootPerScan); ++i) {
            const uint32_t entryPtr = containerEntries + static_cast<uint32_t>(i) * kDictEntrySize;

            int hash = 0;
            if (!Mem.Read(entryPtr + 0x0, hash) || hash < 0)
                continue;

            uint32_t objDict = 0;
            if (!Mem.Read(entryPtr + 0x0C, objDict) || !objDict)
                continue;

            ScanObjectDict(objDict, cameraPos, maxDist, found);
        }
    };

    uint32_t lomPrimary = 0;
    uint32_t lomAlt = 0;
    Mem.Read(currentGame + Offsets::LevelObjectManager, lomPrimary);
    Mem.Read(currentGame + Offsets::LevelObjectManagerAlt, lomAlt);

    if (!lomPrimary && !lomAlt) {
        g_Globals.Loot.GroundLoot.clear();
        return;
    }

    scanLom(lomPrimary);
    if (lomAlt && lomAlt != lomPrimary)
        scanLom(lomAlt);

    g_Globals.Loot.GroundLoot = std::move(found);
    }
    catch (...) {
        g_Globals.Loot.GroundLoot.clear();
    }
}

}
}
