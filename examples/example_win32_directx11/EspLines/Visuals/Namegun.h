#pragma once
#include <vector>
#include <string>
#include <unordered_map>

class Namegun {
public:
    struct GunInfo {
        std::string name;
        std::string icon;
        bool isSpecial = false;
        bool hasLevels = false;
        bool hideEsp = false;
    };

    // Static initialization function
    static void Init();

    // Static weapon name functions
    static std::string GetGunName(short gunId);
    static std::string GetGunIcon(short gunId);
    static std::string GetBaseName(const std::string& fullName);
    static bool HasIcon(short gunId);
    static bool ShouldShowWeapon(short gunId);

    // Static ammo type functions
    static std::string GetAmmoTypeName(short gunId); // Static function for ammo type detection

private:
    static std::vector<GunInfo> GunData;
};