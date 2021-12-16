/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LFG_MGR_H
#define _LFG_MGR_H

#include "Common.h"
#include "Policies/Singleton.h"

#include <map>

struct LFGDungeonEntry;

 /// Reward info
struct LfgReward
{
    LfgReward(uint32 maxLevel = 0, uint32 firstQuest = 0, uint32 otherQuest = 0) :
        maxLevel(maxLevel), firstQuest(firstQuest), otherQuest(otherQuest) { }

    uint32 maxLevel;
    uint32 firstQuest;
    uint32 otherQuest;
};

struct LFGDungeonData
{
    LFGDungeonData();
    LFGDungeonData(LFGDungeonEntry const* dbc);

    uint32 id;
    std::string name;
    uint16 map;
    uint8 type;
    uint8 expansion;
    uint8 group;
    uint8 minlevel;
    uint8 maxlevel;
    Difficulty difficulty;
    bool seasonal;
    float x, y, z, o;

    // Helpers
    uint32 Entry() const { return id + (type << 24); }
};

typedef std::map<uint32, uint32> LfgLockMap;
typedef std::set<uint32> LfgDungeonSet;

class LFGMgr
{
    public:
        void LoadRewards();
        void LoadLFGDungeons(bool reload = false);

        // Checks if Seasonal dungeon is active
        bool IsSeasonActive(uint32 dungeonId) const;

        // Return Lfg dungeon entry for given dungeon id
        uint32 GetLFGDungeonEntry(uint32 id);
        // Gets the random dungeon reward corresponding to given dungeon and player level
        LfgReward const* GetRandomDungeonReward(uint32 dungeon, uint8 level);
        // Returns all random and seasonal dungeons for given level and expansion
        LfgDungeonSet GetRandomAndSeasonalDungeons(uint8 level, uint8 expansion) const;
        /// Get locked dungeons
        LfgLockMap const GetLockedDungeons(Player* player);

        LfgDungeonSet const& GetDungeonsByRandom(uint32 randomdungeon);

    private:
        LFGDungeonData const* GetLFGDungeon(uint32 id);

        std::unordered_map<uint32, LFGDungeonData> m_lfgDungeons;
        std::multimap<uint32, LfgReward> m_lfgRewards;
        std::map<uint32, LfgDungeonSet> m_cachedDungeonMapsPerGroup;
};

#define sLFGMgr MaNGOS::Singleton<LFGMgr>::Instance()

#endif