#include "Namegun.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

// Define the static member variable
std::vector<Namegun::GunInfo> Namegun::GunData{};

void Namegun::Init() {
    static const int kTableVersion = 4;
    static int s_loadedVersion = 0;
    if (s_loadedVersion == kTableVersion && !GunData.empty())
        return;

    GunData.clear();
    GunData.resize(25000);
    s_loadedVersion = kTableVersion;

    // Special
    GunData[6016] = { "CALL AIRDROP", "\ue080", true };
    GunData[10006] = { "TRUCK DROP", "\ue080", true };
    GunData[-15524 + 25000] = { "Healing Mini Drone", "\ue066", true };
    GunData[-15521 + 25000] = { "Healing Mini Drone (1 USE)", "\ue066", true };
    GunData[21001] = { "Healing Pistol", "\ue066", true };
    GunData[21002] = { "M590", "\ue084", true };
    GunData[21020] = { "Fist", "\ue005", true };

    // Bot's
    GunData[0] = { "AK47", "\ue074" };

    // Fuzil 
    GunData[2] = { "M4A1", "\ue075" };
    GunData[80] = { "M4A1-I", "\ue075", false, true };
    GunData[81] = { "M4A1-II", "\ue075", false, true };
    GunData[82] = { "M4A1-III", "\ue075", false, true };
    GunData[6] = { "AK47", "\ue074" };
    GunData[11] = { "M14", "\ue079" };
    GunData[63] = { "M14-I", "\ue079" , false, true };
    GunData[126] = { "M14-II", "\ue079" , false, true };
    GunData[127] = { "M14-III", "\ue079" , false, true };
    GunData[12] = { "SCAR", "\ue078" };
    GunData[178] = { "SCAR-I", "\ue078" , false, true };
    GunData[179] = { "SCAR-II", "\ue078" , false, true };
    GunData[180] = { "SCAR-III", "\ue078" , false, true };
    GunData[14] = { "GROZA", "\ue07b" };
    GunData[70] = { "GROZA-X", "\ue07b", false, true };
    GunData[24] = { "FAMAS", "\ue07a" };
    GunData[67] = { "FAMAS-I", "\ue07a" , false, true };
    GunData[130] = { "FAMAS-II", "\ue07a" , false, true };
    GunData[131] = { "FAMAS-III", "\ue07a" , false, true };
    GunData[28] = { "XM8", "\ue077" };
    GunData[33] = { "AN94", "\ue07d" };
    GunData[39] = { "Plasma", "\ue07c" };
    GunData[46] = { "AUG", "\ue076" };
    GunData[193] = { "AUG-I", "\ue076" , false, true };
    GunData[194] = { "AUG-II", "\ue076" , false, true };
    GunData[195] = { "AUG-III", "\ue076" , false, true };
    GunData[47] = { "PARAFAL", "\ue07f" };
    GunData[150] = { "Bizon", "\uE06C" };
    GunData[73] = { "G36", "\ue087" };
    GunData[74] = { "G36-II", "\ue087", false, true };

    // Fuzil De Atirador 
    GunData[18] = { "SKS", "\ue071" };
    GunData[26] = { "SVD", "\ue070" };
    GunData[72] = { "SVD-Y", "\ue070", false, true };
    GunData[48] = { "Woodpecker", "\ue07e" };
    GunData[57] = { "Woodpecker", "\ue07e" };
    GunData[109] = { "Carbine", "\ue073" };
    GunData[89] = { "AC80", "\ue072" };

    // Metralhadora
    GunData[19] = { "M249", "\ue06f" };
    GunData[71] = { "M249-X", "\ue06f" , false, true };
    GunData[30] = { "M60", "\ue06e" };
    GunData[61] = { "M60-I", "\ue06e" , false, true };
    GunData[122] = { "M60-II", "\ue06e" , false, true };
    GunData[123] = { "M60-III", "\ue06e" , false, true };
    GunData[54] = { "Kord", "\ue06f" };

    // Submetralhadoras (SMG)
    GunData[7] = { "UMP", "\ue063" };
    GunData[8] = { "MP5", "\ue061" };
    GunData[60] = { "MP5-I", "\ue061" , false, true };
    GunData[120] = { "MP5-II", "\ue061" , false, true };
    GunData[121] = { "MP5-III", "\ue061" , false, true };
    GunData[13] = { "VSS", "\ue065" };
    GunData[62] = { "VSS-I", "\ue065" , false, true };
    GunData[124] = { "VSS-II", "\ue065" , false, true };
    GunData[125] = { "VSS-III", "\ue065" , false, true };
    GunData[15] = { "MP40", "\ue064" };
    GunData[32] = { "P90", "\ue069" };
    GunData[35] = { "CG15", "\ue067" };
    GunData[43] = { "Thompson", "\ue06b" };
    GunData[49] = { "Vector", "\ue06a" };
    GunData[69] = { "Double-Vector", "\ue088" };
    GunData[88] = { "MAC10", "\ue06d" };
    GunData[228] = { "MAC10-I", "\ue06d" , false, true };
    GunData[229] = { "MAC10-II", "\ue06d" , false, true };
    GunData[230] = { "MAC10-III", "\ue06d" , false, true };
    // Shotguns
    GunData[5] = { "M1014", "\ue059" };
    GunData[184] = { "M1014-I", "\ue059" , false, true };
    GunData[185] = { "M1014-II", "\ue059" , false, true };
    GunData[186] = { "M1014-III", "\ue059" , false, true };
    GunData[29] = { "SPAS12", "\ue05a" };
    GunData[41] = { "M1887", "\ue05d" };
    GunData[119] = { "M1887-X", "\ue05d" , false, true };
    GunData[50] = { "MAG-7", "\ue05b" };
    GunData[86] = { "Charge Buster", "\ue05d" };
    GunData[181] = { "Trogon-Shotgun", "\ue05e" };
    GunData[182] = { "Trogon-Grenade", "\ue05e" };

    // Fuzil De Precisão
    GunData[4] = { "AWM", "\ue04f" };
    GunData[65] = { "AWM-Y", "\ue04f" , false, true };
    GunData[21] = { "Kar98K", "\ue050" , false, true };
    GunData[64] = { "Kar98K-I", "\ue050" , false, true };
    GunData[128] = { "Kar98K-II", "\ue050", false, true };
    GunData[129] = { "Kar98K-III", "\ue050" , false, true };
    GunData[45] = { "M82B", "\ue053" };
    GunData[75] = { "M24", "\ue054" };
    GunData[78] = { "Heal Sniper", "\ue054" };
    GunData[197] = { "VSK94", "\ue056" };

    // Pistola
    GunData[3] = { "USP", "\ue04e" };
    GunData[56] = { "USP-2", "\ue060" , false, true };
    GunData[9] = { "Desert Eagle", "\ue051" };
    GunData[10] = { "G18", "\ue052" };
    GunData[20] = { "M1873", "\ue057" };
    GunData[25] = { "M500", "\ue058" };
    GunData[55] = { "M1917", "\ue05c" };
    GunData[58] = { "Mini Uzi", "\ue062" };
    GunData[93] = { "Healing Pistol", "\ue066" };

    // Melee
    GunData[16] = { "Pan", "\ue04a" };
    GunData[17] = { "Parang", "\ue04b" };
    GunData[27] = { "Bat", "\ue04c" };
    GunData[34] = { "Katana", "\ue04b" };
    GunData[51] = { "Scythe", "\ue04d" };
    GunData[53] = { "FF Knife", "\ue085" };

    // Fist
    GunData[1] = { "Fist", "\ue005" };

    // Grenades
    GunData[601] = { "Grenade", "\ue080" };
    GunData[603] = { "Smoke Grenade", "\uE086" };
    GunData[602] = { "Flash Grenade", "\ue082" };
    GunData[608] = { "Freeze Bomb", "\ue083" };

    // Launchers / misc
    GunData[23] = { "M79", "\ue081" };
    GunData[36] = { "RGS-50", "\ue053" };
    GunData[37] = { "MGL140", "\ue080" };
    GunData[44] = { "PAR-90", "\ue07f" };
    GunData[196] = { "FGL-24", "\ue081" };

    // Marksman / newer rifles (OB28+)
    GunData[87] = { "AC88", "\ue072" };
    GunData[96] = { "Gatling", "\ue06e" };
    GunData[98] = { "Extra Ammo", "\ue05f" };
    GunData[101] = { "FFARM", "\ue07f" };
    GunData[102] = { "Heal Sniper", "\ue054" }; // alias / alternate id
    GunData[103] = { "Kingfisher", "\ue078" };
    GunData[104] = { "F2000", "\ue075" };
    GunData[90] = { "Winchester", "\ue050" };
    GunData[105] = { "Winchester", "\ue050" };
    GunData[106] = { "Winchester-II", "\ue050", false, true };
    GunData[107] = { "Winchester-III", "\ue050", false, true };

    // Healing / support weapons
    GunData[76] = { "Treatment Gun", "\ue066" };

    // Special ranged (Mamba / event series)
    GunData[136] = { "Crossbow", "\ue071" };
    GunData[137] = { "Hand Cannon", "\ue051" };
    GunData[138] = { "Ice Gun", "\ue083" };
    GunData[139] = { "Hydro Blaster", "\ue089" };
    GunData[142] = { "Rainbow Spray", "\ue068" };

    // Grenades (extended)
    GunData[604] = { "Gloo Melter", "\ue080" };

    // Other / special
    GunData[100] = { "Flamethrower", "\ue068" };
    GunData[99] = { "Shield Gun", "\ue066" };
    GunData[617] = { "Ice Breaker", "\ue083" };
    GunData[1401] = { "Landmines", "\ue080" };
    GunData[1006] = { "Mini Drone", "\ue066" };
    GunData[1015] = { "Supply Crate", "\ue080" };

    // OB40+ / event weapons (best-match icons from weapon icon font)
    GunData[91] = { "Heal Pistol", "\ue066" };
    GunData[92] = { "Hybrid Blaster", "\ue068" };
    GunData[94] = { "Heal UAV", "\ue066" };
    GunData[95] = { "Mini Turret", "\ue06f" };
    GunData[97] = { "Bolt Maker", "\ue05f" };
    GunData[110] = { "Jammer", "\ue080" };
    GunData[111] = { "Horizaline", "\ue080" };
    GunData[112] = { "Super Med Kit", "\ue066" };
    GunData[113] = { "Energizer", "\ue066" };
    GunData[114] = { "Launch Pad", "\ue080" };
    GunData[115] = { "Binoculars", "\ue080" };
    GunData[116] = { "Helper Bot", "\ue066" };
    GunData[117] = { "UAV", "\ue066" };
    GunData[118] = { "Bow", "\ue071" };
    GunData[132] = { "Water Gun", "\ue089" };
    GunData[133] = { "Super Bonfire", "\ue068" };
    GunData[134] = { "Gloo Wall", "\ue080" };
    GunData[135] = { "Ice Wall", "\ue083" };
    GunData[140] = { "Gloo Melter", "\ue080" };
    GunData[141] = { "Repair Kit", "\ue066" };
    GunData[143] = { "Paint Spray", "\ue068" };
    GunData[144] = { "Stun Gun", "\ue062" };
    GunData[145] = { "Magnetic Gun", "\ue089" };
}

static int ResolveGunTableId(short gunId) {
    int adjustedId = gunId < 0 ? gunId + 25000 : static_cast<int>(gunId);

    if (adjustedId > 0xFFFF) {
        const int low = adjustedId & 0xFFFF;
        if (low > 0 && low < 500)
            return low;
    }

    if (adjustedId >= 20000 && adjustedId < 21000) {
        const int sub = adjustedId - 20000;
        if (sub > 0)
            return sub;
    }

    if (adjustedId >= 21000 && adjustedId < 22000) {
        const int sub = adjustedId - 21000;
        if (sub == 1) return 21001;
        if (sub == 2) return 21002;
        if (sub == 6) return 21006;
        if (sub == 20) return 1; // Fist
        // Ground loot / pickup encoding: 21004 -> AWM, 21065 -> AWM-Y, etc.
        if (sub > 0 && sub < 25000)
            return sub;
    }

    return adjustedId;
}

bool Namegun::ShouldShowWeapon(short gunId) {
    try {
        const int adjustedId = ResolveGunTableId(gunId);
        if (adjustedId < 0 || adjustedId >= static_cast<int>(GunData.size()))
            return false;
        return !GunData[adjustedId].hideEsp;
    }
    catch (const std::exception&) {
        return false;
    }
}

std::string Namegun::GetGunName(short gunId) {
    try {
        const int adjustedId = ResolveGunTableId(gunId);

        if (adjustedId >= 0 && adjustedId < static_cast<int>(GunData.size()) &&
            !GunData[adjustedId].hideEsp &&
            (!GunData[adjustedId].name.empty() || GunData[adjustedId].isSpecial)) {
            return GunData[adjustedId].name;
        }

        return {};
    }
    catch (const std::exception&) {
        return {};
    }
}

std::string Namegun::GetGunIcon(short gunId) {
    try {
        int adjustedId = ResolveGunTableId(gunId);

        if (adjustedId >= 0 && adjustedId < GunData.size()) {
            if (GunData[adjustedId].hideEsp)
                return "";
            if (!GunData[adjustedId].icon.empty()) {
                if (GunData[adjustedId].hasLevels) {
                    std::string baseName = GetBaseName(GunData[adjustedId].name);
                    std::string suffix = GunData[adjustedId].name.substr(baseName.length());
                    return GunData[adjustedId].icon + suffix;
                }
                return GunData[adjustedId].icon;
            }

            // Named weapon with no dedicated glyph: pick closest family icon
            const std::string& name = GunData[adjustedId].name;
            if (!name.empty()) {
                if (name.find("AWM") != std::string::npos || name.find("Kar98") != std::string::npos ||
                    name.find("M82") != std::string::npos || name.find("M24") != std::string::npos ||
                    name.find("Sniper") != std::string::npos || name.find("SVD") != std::string::npos ||
                    name.find("VSK") != std::string::npos || name.find("Winchester") != std::string::npos)
                    return "\ue04f";
                if (name.find("M1014") != std::string::npos || name.find("SPAS") != std::string::npos ||
                    name.find("M1887") != std::string::npos || name.find("MAG") != std::string::npos ||
                    name.find("Shotgun") != std::string::npos || name.find("Trogon") != std::string::npos ||
                    name.find("Charge Buster") != std::string::npos || name.find("M590") != std::string::npos)
                    return "\ue059";
                if (name.find("UMP") != std::string::npos || name.find("MP5") != std::string::npos ||
                    name.find("MP40") != std::string::npos || name.find("Thompson") != std::string::npos ||
                    name.find("P90") != std::string::npos || name.find("Vector") != std::string::npos ||
                    name.find("MAC10") != std::string::npos || name.find("VSS") != std::string::npos)
                    return "\ue063";
                if (name.find("M249") != std::string::npos || name.find("M60") != std::string::npos ||
                    name.find("Kord") != std::string::npos || name.find("Gatling") != std::string::npos)
                    return "\ue06f";
                if (name.find("Grenade") != std::string::npos || name.find("Bomb") != std::string::npos ||
                    name.find("M79") != std::string::npos || name.find("MGL") != std::string::npos ||
                    name.find("FGL") != std::string::npos || name.find("RGS") != std::string::npos ||
                    name.find("Landmine") != std::string::npos || name.find("Airdrop") != std::string::npos)
                    return "\ue080";
                if (name.find("Pan") != std::string::npos || name.find("Parang") != std::string::npos ||
                    name.find("Machete") != std::string::npos || name.find("Katana") != std::string::npos ||
                    name.find("Scythe") != std::string::npos || name.find("Bat") != std::string::npos)
                    return "\ue04a";
                if (name.find("Heal") != std::string::npos || name.find("Treatment") != std::string::npos ||
                    name.find("Drone") != std::string::npos || name.find("UAV") != std::string::npos)
                    return "\ue066";
                if (name.find("Flame") != std::string::npos || name.find("Spray") != std::string::npos ||
                    name.find("Hydro") != std::string::npos || name.find("Ice Gun") != std::string::npos)
                    return "\ue068";
                if (name.find("Crossbow") != std::string::npos || name.find("Bow") != std::string::npos ||
                    name.find("SKS") != std::string::npos || name.find("Carbine") != std::string::npos)
                    return "\ue071";
                if (name.find("Shield") != std::string::npos)
                    return "\ue066";
                return "\ue074"; // default AR-style glyph
            }
        }
        return "";
    }
    catch (const std::exception& ex) {
        std::cout << "Erro ao obter ícone da arma: " << ex.what() << std::endl;
        return "Unknown";
    }
}

std::string Namegun::GetBaseName(const std::string& fullName) {
    size_t dashPos = fullName.find_last_of('-');
    if (dashPos != std::string::npos) {
        std::string suffix = fullName.substr(dashPos + 1);
        if (suffix == "I" || suffix == "II" || suffix == "III" ||
            suffix == "X" || suffix == "Y" || suffix == "2") {
            return fullName.substr(0, dashPos);
        }
    }
    return fullName;
}

bool Namegun::HasIcon(short gunId) {
    int adjustedId = ResolveGunTableId(gunId);
    return (adjustedId >= 0 && adjustedId < GunData.size() &&
        !GunData[adjustedId].hideEsp &&
        !GunData[adjustedId].icon.empty());
}

// Static function to get ammo type name from weapon ID
std::string Namegun::GetAmmoTypeName(short gunId) {
    try {
        int adjustedId = gunId < 0 ? gunId + 25000 : gunId;

        if (adjustedId >= 0 && adjustedId < GunData.size() &&
            (!GunData[adjustedId].name.empty() || GunData[adjustedId].isSpecial)) {

            std::string gunName = GunData[adjustedId].name;

            // Convert weapon names to ammo type names
            if (gunName.find("AK47") != std::string::npos ||
                gunName.find("M4A1") != std::string::npos ||
                gunName.find("SCAR") != std::string::npos ||
                gunName.find("GROZA") != std::string::npos ||
                gunName.find("FAMAS") != std::string::npos ||
                gunName.find("XM8") != std::string::npos ||
                gunName.find("AN94") != std::string::npos ||
                gunName.find("Plasma") != std::string::npos ||
                gunName.find("AUG") != std::string::npos ||
                gunName.find("M14") != std::string::npos) {
                return "AR Ammo";
            }
            else if (gunName.find("UMP9") != std::string::npos ||
                gunName.find("MP40") != std::string::npos ||
                gunName.find("MP5") != std::string::npos ||
                gunName.find("Thompson") != std::string::npos ||
                gunName.find("P90") != std::string::npos ||
                gunName.find("Vector") != std::string::npos ||
                gunName.find("UZI") != std::string::npos) {
                return "SMG Ammo";
            }
            else if (gunName.find("AWM") != std::string::npos ||
                gunName.find("M82") != std::string::npos ||
                gunName.find("Kar98k") != std::string::npos ||
                gunName.find("SVD") != std::string::npos ||
                gunName.find("M24") != std::string::npos ||
                gunName.find("SKS") != std::string::npos ||
                gunName.find("VSS") != std::string::npos) {
                return "Sniper Ammo";
            }
            else if (gunName.find("M1014") != std::string::npos ||
                gunName.find("M1887") != std::string::npos ||
                gunName.find("SPAS12") != std::string::npos ||
                gunName.find("M590") != std::string::npos ||
                gunName.find("AA12") != std::string::npos) {
                return "Shotgun Ammo";
            }
            else if (gunName.find("Glock") != std::string::npos ||
                gunName.find("Desert Eagle") != std::string::npos ||
                gunName.find("P92") != std::string::npos ||
                gunName.find("USP") != std::string::npos ||
                gunName.find("M1911") != std::string::npos) {
                return "Pistol Ammo";
            }
            else if (gunName.find("M60") != std::string::npos ||
                gunName.find("MG3") != std::string::npos ||
                gunName.find("RPK") != std::string::npos ||
                gunName.find("M249") != std::string::npos) {
                return "LMG Ammo";
            }
            else if (gunName.find("Crossbow") != std::string::npos ||
                gunName.find("Bow") != std::string::npos) {
                return "Arrow";
            }
            else if (gunName.find("RPG") != std::string::npos ||
                gunName.find("M79") != std::string::npos ||
                gunName.find("M32") != std::string::npos) {
                return "Explosive";
            }
            else {
                return "Unknown Ammo";
            }
        }

        return "Unknown Ammo";
    }
    catch (const std::exception& ex) {
        std::cout << "Erro ao obter tipo de munição: " << ex.what() << std::endl;
        return "Unknown Ammo";
    }
}

