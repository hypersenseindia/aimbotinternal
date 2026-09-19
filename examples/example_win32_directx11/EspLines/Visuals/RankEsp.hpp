#pragma once

#include <string>
#include <examples/example_win32_directx11/EspLines/Memory/Memory.hpp>
#include <examples/example_win32_directx11/EspLines/Offsets.hpp>

namespace RankEsp {

inline std::string GetRankName(int points, int /*rankId*/)
{
    std::string rankStr = "Unknown";

    if (points < 1300)
        rankStr = "Bronze";
    else if (points < 1600)
        rankStr = "Silver";
    else if (points < 2100)
        rankStr = "Gold";
    else if (points < 2600) {
        if (points < 2225)
            rankStr = "Platinum I";
        else if (points < 2350)
            rankStr = "Platinum II";
        else if (points < 2475)
            rankStr = "Platinum III";
        else
            rankStr = "Platinum IV";
    }
    else if (points < 3200) {
        if (points < 2750)
            rankStr = "Diamond I";
        else if (points < 2900)
            rankStr = "Diamond II";
        else if (points < 3050)
            rankStr = "Diamond III";
        else
            rankStr = "Diamond IV";
    }
    else if (points < 6000)
        rankStr = "Heroic";
    else
        rankStr = "Grandmaster";

    return rankStr + " (" + std::to_string(points) + ")";
}

inline bool ReadRankFromEntity(uint32_t entity, std::string& outRank)
{
    if (entity == 0) {
        outRank.clear();
        return false;
    }

    uint32_t profilePtr = 0;
    if (!Mem.Read(entity + Offsets::Player_Profile, profilePtr) || profilePtr == 0) {
        outRank = "Unknown";
        return false;
    }

    const int rankId = Mem.ReadS<int>(profilePtr + Offsets::Profile_Rank);
    const int rankPoints = Mem.ReadS<int>(profilePtr + Offsets::Profile_RankPoint);
    outRank = GetRankName(rankPoints, rankId);
    return true;
}

} // namespace RankEsp
