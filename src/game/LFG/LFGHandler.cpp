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

#include "Server/WorldSession.h"
#include "Log.h"
#include "Entities/Player.h"
#include "WorldPacket.h"
#include "Globals/ObjectAccessor.h"
#include "LFG/LFGMgr.h"

void BuildPlayerLockDungeonBlock(WorldPacket& data, LfgLockMap const& lock)
{
    data << uint32(lock.size());                           // Size of lock dungeons
    for (auto itr = lock.begin(); itr != lock.end(); ++itr)
    {
        data << uint32(itr->first);                         // Dungeon entry (id + type)
        data << uint32(itr->second);                        // Lock status
    }
}

void WorldSession::HandleLfgJoinOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_JOIN");

    uint32 roles;
    uint8 noPartialClear;
    uint8 achievements;
    uint8 slotsCount;
    uint8 needCount;

    std::string comment;
    std::vector<uint32> slots;
    std::vector<uint32> needs;

    recv_data >> roles;
    recv_data >> noPartialClear;
    recv_data >> achievements;

    recv_data >> slotsCount;

    slots.resize(slotsCount);

    for (uint8 i = 0; i < slotsCount; ++i)
        recv_data >> slots[i];

    recv_data >> needCount;

    needs.resize(needCount);

    for (uint8 i = 0; i < needCount; ++i)
        recv_data >> needs[i];

    recv_data >> comment;

    // SendLfgJoinResult(ERR_LFG_OK);
    // SendLfgUpdate(false, LFG_UPDATE_JOIN, dungeons[0]);
}

void WorldSession::HandleLfgLeaveOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_LFG_LEAVE");

    // SendLfgUpdate(false, LFG_UPDATE_LEAVE, 0);
}

void WorldSession::HandleLfgProposalResultOpcode(WorldPacket& recv_data)
{
    uint32 lfgGroupID; // Internal lfgGroupID
    bool accept; // Accept to join?
    recv_data >> lfgGroupID;
    recv_data >> accept;
}

void WorldSession::HandleLfgSetRolesOpcode(WorldPacket& recv_data)
{
    uint8 roles;
    recv_data >> roles; // Player Group Roles
}

void WorldSession::HandleLfgSetCommentOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_SET_LFG_COMMENT");

    std::string comment;
    recv_data >> comment;
    DEBUG_LOG("LFG comment \"%s\"", comment.c_str());
}

void WorldSession::HandleLfgSetBootVoteOpcode(WorldPacket& recv_data)
{
    bool agree; // Agree to kick player
    recv_data >> agree;
}

void WorldSession::HandleLfrJoinOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_SEARCH_JOIN");

    uint32 entry; // Raid id to search
    recv_data >> entry;
}

void WorldSession::HandleLfrLeaveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_SEARCH_LEAVE");

    uint32 dungeonId; // Raid id queue to leave
    recv_data >> dungeonId;
}

void WorldSession::HandleLfgGetStatus(WorldPacket& recv_data)
{
}

void WorldSession::HandleLfgTeleportOpcode(WorldPacket& recv_data)
{
    bool out;
    recv_data >> out;
}

void WorldSession::HandleLfgPlayerLockInfoRequestOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_PLAYER_LOCK_INFO_REQUEST %s", GetPlayer()->GetName());

    // Get Random dungeons that can be done at a certain level and expansion
    uint8 level = GetPlayer()->GetLevel();
    LfgDungeonSet const& randomDungeons = sLFGMgr.GetRandomAndSeasonalDungeons(level, GetPlayer()->GetSession()->GetExpansion());

    // Get player locked Dungeons
    LfgLockMap const& lock = sLFGMgr.GetLockedDungeons(GetPlayer());
    uint32 rsize = uint32(randomDungeons.size());
    uint32 lsize = uint32(lock.size());

    DEBUG_LOG("SMSG_LFG_PLAYER_INFO %s", GetPlayer()->GetName());
    WorldPacket data(SMSG_LFG_PLAYER_INFO, 1 + rsize * (4 + 1 + 4 + 4 + 4 + 4 + 1 + 4 + 4 + 4) + 4 + lsize * (1 + 4 + 4 + 4 + 4 + 1 + 4 + 4 + 4));

    data << uint8(randomDungeons.size());                  // Random Dungeon count
    for (auto itr = randomDungeons.begin(); itr != randomDungeons.end(); ++itr)
    {
        data << uint32(*itr);                               // Dungeon Entry (id + type)
        LfgReward const* reward = sLFGMgr.GetRandomDungeonReward(*itr, level);
        Quest const* quest = nullptr;
        bool done = false;
        if (reward)
        {
            quest = sObjectMgr.GetQuestTemplate(reward->firstQuest);
            if (quest)
            {
                done = !GetPlayer()->CanRewardQuest(quest, false);
                if (done)
                    quest = sObjectMgr.GetQuestTemplate(reward->otherQuest);
            }
        }

        if (quest)
        {
            data << uint8(done);
            data << uint32(quest->GetRewOrReqMoney());
            data << uint32(quest->XPValue(GetPlayer()));
            data << uint32(0); // money variance per missing member when queueing - not actually used back then
            data << uint32(0); // experience variance per missing member when queueing - not actually used back then
            data << uint8(quest->GetRewItemsCount());
            if (quest->GetRewItemsCount())
            {
                for (uint8 i = 0; i < QUEST_REWARDS_COUNT; ++i)
                    if (uint32 itemId = quest->RewItemId[i])
                    {
                        ItemPrototype const* item = sObjectMgr.GetItemPrototype(itemId);
                        data << uint32(itemId);
                        data << uint32(item ? item->DisplayInfoID : 0);
                        data << uint32(quest->RewItemCount[i]);
                    }
            }
        }
        else
        {
            data << uint8(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint8(0);
        }
    }
    BuildPlayerLockDungeonBlock(data, lock);
    SendPacket(data);
}

void WorldSession::HandleLfgPartyLockInfoRequestOpcode(WorldPacket& recv_data)
{
}

void WorldSession::SendLfgSearchResults(LfgType type, uint32 entry) const
{
    WorldPacket data(SMSG_LFG_SEARCH_RESULTS);
    data << uint32(type);                                   // type
    data << uint32(entry);                                  // entry from LFGDungeons.dbc

    uint8 isGuidsPresent = 0;
    data << uint8(isGuidsPresent);
    if (isGuidsPresent)
    {
        uint32 guids_count = 0;
        data << uint32(guids_count);
        for (uint32 i = 0; i < guids_count; ++i)
        {
            data << uint64(0);                              // player/group guid
        }
    }

    uint32 groups_count = 1;
    data << uint32(groups_count);                           // groups count
    data << uint32(groups_count);                           // groups count (total?)

    for (uint32 i = 0; i < groups_count; ++i)
    {
        data << uint64(1);                                  // group guid

        uint32 flags = 0x92;
        data << uint32(flags);                              // flags

        if (flags & 0x2)
        {
            data << uint8(0);                               // comment string, max len 256
        }

        if (flags & 0x10)
        {
            for (uint32 j = 0; j < 3; ++j)
                data << uint8(0);                           // roles
        }

        if (flags & 0x80)
        {
            data << uint64(0);                              // instance guid
            data << uint32(0);                              // completed encounters
        }
    }

    // TODO: Guard Player map
    HashMapHolder<Player>::MapType const& players = sObjectAccessor.GetPlayers();
    uint32 playersSize = players.size();
    data << uint32(playersSize);                            // players count
    data << uint32(playersSize);                            // players count (total?)

    for (const auto& player : players)
    {
        Player* plr = player.second;

        if (!plr || plr->GetTeam() != _player->GetTeam())
            continue;

        if (!plr->IsInWorld())
            continue;

        data << plr->GetObjectGuid();                       // guid

        uint32 flags = 0xFF;
        data << uint32(flags);                              // flags

        if (flags & 0x1)
        {
            data << uint8(plr->GetLevel());
            data << uint8(plr->getClass());
            data << uint8(plr->getRace());

            for (uint32 i = 0; i < 3; ++i)
                data << uint8(0);                           // talent spec x/x/x

            data << uint32(0);                              // armor
            data << uint32(0);                              // spd/heal
            data << uint32(0);                              // spd/heal
            data << uint32(0);                              // HasteMelee
            data << uint32(0);                              // HasteRanged
            data << uint32(0);                              // HasteSpell
            data << float(0);                               // MP5
            data << float(0);                               // MP5 Combat
            data << uint32(0);                              // AttackPower
            data << uint32(0);                              // Agility
            data << uint32(0);                              // Health
            data << uint32(0);                              // Mana
            data << uint32(0);                              // Unk1
            data << float(0);                               // Unk2
            data << uint32(0);                              // Defence
            data << uint32(0);                              // Dodge
            data << uint32(0);                              // Block
            data << uint32(0);                              // Parry
            data << uint32(0);                              // Crit
            data << uint32(0);                              // Expertise
        }

        if (flags & 0x2)
            data << "";                                     // comment

        if (flags & 0x4)
            data << uint8(0);                               // group leader

        if (flags & 0x8)
            data << uint64(1);                              // group guid

        if (flags & 0x10)
            data << uint8(0);                               // roles

        if (flags & 0x20)
            data << uint32(plr->GetZoneId());               // areaid

        if (flags & 0x40)
            data << uint8(0);                               // status

        if (flags & 0x80)
        {
            data << uint64(0);                              // instance guid
            data << uint32(0);                              // completed encounters
        }
    }

    SendPacket(data);
}

void WorldSession::SendLfgJoinResult(LfgJoinResult result) const
{
    WorldPacket data(SMSG_LFG_JOIN_RESULT, 0);
    data << uint32(result);
    data << uint32(0); // ERR_LFG_ROLE_CHECK_FAILED_TIMEOUT = 3, ERR_LFG_ROLE_CHECK_FAILED_NOT_VIABLE = (value - 3 == result)

    if (result == ERR_LFG_NO_SLOTS_PARTY)
    {
        uint8 count1 = 0;
        data << uint8(count1);                              // players count?
        /*for (uint32 i = 0; i < count1; ++i)
        {
            data << uint64(0);                              // player guid?
            uint32 count2 = 0;
            for (uint32 j = 0; j < count2; ++j)
            {
                data << uint32(0);                          // dungeon id/type
                data << uint32(0);                          // lock status?
            }
        }*/
    }

    SendPacket(data);
}

void WorldSession::SendLfgUpdate(bool isGroup, LfgUpdateType updateType, uint32 id) const
{
    WorldPacket data(isGroup ? SMSG_LFG_UPDATE_PARTY : SMSG_LFG_UPDATE_PLAYER, 0);
    data << uint8(updateType);

    uint8 extra = updateType == LFG_UPDATE_JOIN ? 1 : 0;
    data << uint8(extra);

    if (extra)
    {
        data << uint8(0);
        data << uint8(0);
        data << uint8(0);

        if (isGroup)
        {
            data << uint8(0);
            for (uint32 i = 0; i < 3; ++i)
                data << uint8(0);
        }

        uint8 count = 1;
        data << uint8(count);
        for (uint32 i = 0; i < count; ++i)
            data << uint32(id);
        data << "";
    }
    SendPacket(data);
}

void WorldSession::SendLfgDisabled()
{
    DEBUG_LOG("SMSG_LFG_DISABLED");
    WorldPacket data(SMSG_LFG_DISABLED, 0);
    SendPacket(data);
}

void WorldSession::SendLfgOfferContinue(uint32 dungeonEntry)
{
    DEBUG_LOG("SMSG_LFG_OFFER_CONTINUE %u", dungeonEntry);
    WorldPacket data(SMSG_LFG_OFFER_CONTINUE, 4);
    data << uint32(dungeonEntry);
    SendPacket(data);
}

void WorldSession::SendLfgTeleportError(uint8 err)
{
    DEBUG_LOG("SMSG_LFG_TELEPORT_DENIED %u", err);
    WorldPacket data(SMSG_LFG_TELEPORT_DENIED, 4);
    data << uint32(err); // Error
    SendPacket(data);
}

