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

#include "Entities/Object.h"
#include "Entities/Player.h"
#include "BattleGround.h"
#include "BattleGroundWS.h"
#include "Entities/GameObject.h"
#include "Globals/ObjectMgr.h"
#include "BattleGroundMgr.h"
#include "WorldPacket.h"

BattleGroundWS::BattleGroundWS() : m_reputationCapture(0), m_honorWinKills(0), m_honorEndKills(0), m_endTimer(0), m_lastCapturedFlagTeam()
{
    // set battleground start message ids
    m_startMessageIds[BG_STARTING_EVENT_FIRST]  = 0;
    m_startMessageIds[BG_STARTING_EVENT_SECOND] = LANG_BG_WS_START_ONE_MINUTE;
    m_startMessageIds[BG_STARTING_EVENT_THIRD]  = LANG_BG_WS_START_HALF_MINUTE;
    m_startMessageIds[BG_STARTING_EVENT_FOURTH] = LANG_BG_WS_HAS_BEGUN;
}

void BattleGroundWS::Update(uint32 diff)
{
    BattleGround::Update(diff);

    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // compute flag respawn timers
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        // respawn flag after team score
        if (m_flagState[i] == BG_WS_FLAG_STATE_WAIT_RESPAWN && m_flagState[i])
        {
            if (m_flagsTimer[i] <= diff)
            {
                Team playerTeam = GetTeamIdByTeamIndex((PvpTeamIndex)i);
                RespawnFlagAtBase(playerTeam, true);
                m_flagsTimer[i] = 0;
            }
            else
                m_flagsTimer[i] -= diff;
        }
        // respawn flag after dropped
        else if (m_flagState[i] == BG_WS_FLAG_STATE_ON_GROUND && m_flagsDropTimer[i])
        {
            if (m_flagsDropTimer[i] <= diff)
            {
                Team playerTeam = GetTeamIdByTeamIndex((PvpTeamIndex)i);
                m_flagsDropTimer[i] = 0;
                RespawnDroppedFlag(playerTeam);
            }
            else
                m_flagsDropTimer[i] -= diff;
        }
    }

    // Count the renaining time; update world state and end battleground if timer is expired
    if (m_endTimer <= diff)
    {
        if (m_teamScores[TEAM_INDEX_ALLIANCE] > m_teamScores[TEAM_INDEX_HORDE])
            EndBattleGround(ALLIANCE);
        else if (m_teamScores[TEAM_INDEX_ALLIANCE] < m_teamScores[TEAM_INDEX_HORDE])
            EndBattleGround(HORDE);
        else
        {
            // if 0 => tie
            EndBattleGround(m_lastCapturedFlagTeam);
        }
    }
    else
    {
        uint32 minutesLeftPrev = GetRemainingTimeInMinutes();
        m_endTimer -= diff;
        uint32 minutesLeft = GetRemainingTimeInMinutes();

        if (minutesLeft != minutesLeftPrev)
            UpdateWorldState(BG_WS_STATE_TIME_REMAINING, minutesLeft);
    }

    // as long as both flags are missing from bases, progress the FC debuff timer until 15 minutes have elapsed
    if (m_flagState[TEAM_INDEX_ALLIANCE] && (m_flagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_PLAYER || m_flagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_GROUND) &&
        m_flagState[TEAM_INDEX_HORDE] && (m_flagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_PLAYER || m_flagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_GROUND))
    {
        if (m_flagCarrierDebuffTimer)
        {
            if (m_flagCarrierDebuffTimer <= BG_WS_FOCUSED_ASSAULT_TIME && !m_focusedAssaultActive)
            {
                for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
                {
                    Team playerTeam = GetTeamIdByTeamIndex((PvpTeamIndex)i);
                    if (IsFlagPickedUp(playerTeam))
                        if (Player* player = GetBgMap()->GetPlayer(GetFlagCarrierGuid((PvpTeamIndex)i)))
                            player->CastSpell(player, BG_WS_SPELL_FOCUSED_ASSAULT, TRIGGERED_OLD_TRIGGERED);
                }
                m_focusedAssaultActive = true;
            }
            if (m_flagCarrierDebuffTimer <= diff)
            {
                for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
                {
                    Team playerTeam = GetTeamIdByTeamIndex((PvpTeamIndex)i);
                    if (IsFlagPickedUp(playerTeam))
                    {
                        if (Player* player = GetBgMap()->GetPlayer(GetFlagCarrierGuid((PvpTeamIndex)i)))
                        {
                            player->RemoveAurasDueToSpell(BG_WS_SPELL_FOCUSED_ASSAULT);
                            player->CastSpell(player, BG_WS_SPELL_BRUTAL_ASSAULT, TRIGGERED_OLD_TRIGGERED);
                        }
                    }
                }
                m_focusedAssaultActive = false;
                m_brutalAssaultActive = true;
                m_flagCarrierDebuffTimer = 0; // stop timer here for now - no reason to resume until the current stalemate is over and another begins (capture occurs, or both flags are returned?)
            }
            else
                m_flagCarrierDebuffTimer -= diff;
        }
    }
}

void BattleGroundWS::StartingEventOpenDoors()
{
    OpenDoorEvent(BG_EVENT_DOOR);

    // TODO implement timer to despawn doors after a short while

    SpawnEvent(WS_EVENT_SPIRITGUIDES_SPAWN, 0, true);
    SpawnEvent(WS_EVENT_FLAG_A, 0, true);
    SpawnEvent(WS_EVENT_FLAG_H, 0, true);

    // Players that join battleground after start are not eligible to get achievement.
    StartTimedAchievement(ACHIEVEMENT_CRITERIA_TYPE_WIN_BG, WS_TIMED_ACHIEV_WARSONG_EXP);

    // setup graveyards
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_ALLIANCE,     BG_WS_ZONE_ID_MAIN, ALLIANCE);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_HORDE,        BG_WS_ZONE_ID_MAIN, HORDE);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_ALLIANCE, BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_HORDE,    BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
}

void BattleGroundWS::AddPlayer(Player* player)
{
    BattleGround::AddPlayer(player);

    // create score and add it to map, default values are set in constructor
    BattleGroundWGScore* score = new BattleGroundWGScore;

    m_playerScores[player->GetObjectGuid()] = score;
}

// Respawn the flag at the base
void BattleGroundWS::RespawnFlagAtBase(Team team, bool wasCaptured)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);
    m_flagState[teamIdx] = BG_WS_FLAG_STATE_ON_BASE;
    SpawnEvent(teamIdx, 0, true);

    // if flag was captured process the following events
    if (wasCaptured)
    {
        DEBUG_LOG("BattleGroundWS: The main flag for team %u has respawned after enemy score.", team);

        m_flagCarrierDebuffTimer = BG_WS_BRUTAL_ASSAULT_TIME;
        m_brutalAssaultActive = false;
        m_focusedAssaultActive = false;

        // when map_update will be allowed for battlegrounds this code will be useless
        SpawnEvent(WS_EVENT_FLAG_A, 0, true);
        SpawnEvent(WS_EVENT_FLAG_H, 0, true);

        SendMessageToAll(LANG_BG_WS_F_PLACED, CHAT_MSG_BG_SYSTEM_NEUTRAL);
        PlaySoundToAll(BG_WS_SOUND_FLAGS_RESPAWNED);        // flag respawned sound...
    }
    // if both team flags have been returned, reset FC debuffs like a capture would (TODO: confirm this and remove if required)
    else if (m_flagState[otherTeamIdx] == BG_WS_FLAG_STATE_ON_BASE)
    {
        m_flagCarrierDebuffTimer = BG_WS_BRUTAL_ASSAULT_TIME;
        m_brutalAssaultActive = false;
        m_focusedAssaultActive = false;
    }
}

// Respawn the flag at the base
void BattleGroundWS::RespawnDroppedFlag(Team team)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    DEBUG_LOG("BattleGroundWS: The dropped flag for team %u has respawned.", team);

    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);

    // respawn the flag at the base
    RespawnFlagAtBase(team, false);

    // inform players
    SendMessageToAll(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_RESPAWN].messageId, wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_RESPAWN].chatType);
    PlaySoundToAll(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_RESPAWN].soundId);

    // despawn the temp summoned flag
    if (GameObject* flag = GetBgMap()->GetGameObject(GetDroppedFlagGuid(team)))
        flag->SetLootState(GO_JUST_DEACTIVATED);

    ClearDroppedFlagGuid(team);
}

// Method that process captured flag score
void BattleGroundWS::ProcessPlayerFlagScoreEvent(Player* player)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    DEBUG_LOG("BattleGroundWS: Team %u has scored.", player->GetTeam());

    player->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);

    Team team = player->GetTeam();
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    uint8 otherTeamIdx = GetOtherTeamIndex(teamIdx);

    if (!IsFlagPickedUp(otherTeamIdx))
        return;

    ClearFlagCarrier(otherTeamIdx); // must be before aura remove to prevent 2 events (drop+capture) at the same time

    // Horde flag in base (but not respawned yet)
    m_flagState[otherTeamIdx] = BG_WS_FLAG_STATE_WAIT_RESPAWN;

    // Drop Horde Flag from Player
    player->RemoveAurasDueToSpell(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId);
    player->RemoveAurasDueToSpell(BG_WS_SPELL_FOCUSED_ASSAULT);
    player->RemoveAurasDueToSpell(BG_WS_SPELL_BRUTAL_ASSAULT);

    // play sound and reward reputation
    PlaySoundToAll(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_CAPTURED].soundId);
    RewardReputationToTeam(team == ALLIANCE ? 890 : 889, m_reputationCapture, team);

    // for flag capture is reward 2 honorable kills
    RewardHonorToTeam(GetBonusHonorFromKill(2), team);

    // update score
    m_teamScores[teamIdx] += 1;

    // despawn flags
    SpawnEvent(WS_EVENT_FLAG_A, 0, false);
    SpawnEvent(WS_EVENT_FLAG_H, 0, false);

    SendMessageToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_CAPTURED].messageId, wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_CAPTURED].chatType, player);

    UpdateFlagState(team, BG_WS_FLAG_STATE_WAIT_RESPAWN);   // flag state none
    UpdateTeamScore(team);

    // only flag capture should be updated
    UpdatePlayerScore(player, SCORE_FLAG_CAPTURES, 1);      // +1 flag captures

    // Process match winner
    Team winner = m_teamScores[teamIdx] == BG_WS_MAX_TEAM_SCORE ? GetTeamIdByTeamIndex(teamIdx) : TEAM_NONE;

    // update achievements
    player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, 42);

    // end battleground or set respawn timer
    if (winner)
    {
        UpdateWorldState(BG_WS_STATE_FLAG_UNK_ALLIANCE, 0);
        UpdateWorldState(BG_WS_STATE_FLAG_UNK_HORDE, 0);

        UpdateWorldState(BG_WS_STATE_FLAG_ALLIANCE, BG_WS_FLAG_STATE_WAIT_RESPAWN);
        UpdateWorldState(BG_WS_STATE_FLAG_HORDE, BG_WS_FLAG_STATE_WAIT_RESPAWN);

        EndBattleGround(winner);
    }
    else
        m_flagsTimer[otherTeamIdx] = BG_WS_FLAG_RESPAWN_TIME;
}

// Method that handles the flag drop
void BattleGroundWS::HandlePlayerDroppedFlag(Player* player)
{
    DEBUG_LOG("BattleGroundWS: Team %u has dropped the flag.", player->GetTeam());

    Team team = player->GetTeam();
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);

    player->RemoveAurasDueToSpell(BG_WS_SPELL_FOCUSED_ASSAULT);
    player->RemoveAurasDueToSpell(BG_WS_SPELL_BRUTAL_ASSAULT);

    if (GetStatus() != STATUS_IN_PROGRESS)
    {
        if (IsFlagPickedUp(teamIdx) && GetFlagCarrierGuid(teamIdx) == player->GetObjectGuid())
        {
            ClearFlagCarrier(teamIdx);
            player->RemoveAurasDueToSpell(wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId);
        }
    }
    // Check if the other team has picked the flag and if the flag carrier stored is the player which triggers this function
    else if (IsFlagPickedUp(otherTeamIdx) && GetFlagCarrierGuid(otherTeamIdx) == player->GetObjectGuid())
    {
        ClearFlagCarrier(otherTeamIdx);
        player->RemoveAurasDueToSpell(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId);

        m_flagState[otherTeamIdx] = BG_WS_FLAG_STATE_ON_GROUND;
        player->CastSpell(player, wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_DROPPED].spellId, TRIGGERED_OLD_TRIGGERED);

        player->CastSpell(player, SPELL_RECENTLY_DROPPED_FLAG, TRIGGERED_OLD_TRIGGERED);
        UpdateFlagState(team, BG_WS_FLAG_STATE_WAIT_RESPAWN);

        SendMessageToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_DROPPED].messageId, wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_DROPPED].chatType, player);

        UpdateWorldState(wsStateUpdateId[otherTeamIdx], uint32(-1));
        m_flagsDropTimer[otherTeamIdx] = BG_WS_FLAG_DROP_TIME;
    }
}

// Function that handles the flag pick up from the base
void BattleGroundWS::ProcessFlagPickUpFromBase(Player* player, Team attackerTeam)
{
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(attackerTeam);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);

    if (m_flagState[otherTeamIdx] != BG_WS_FLAG_STATE_ON_BASE)
        return;

    DEBUG_LOG("BattleGroundWS: Team %u has taken the enemy flag.", attackerTeam);

    SpawnEvent(otherTeamIdx, 0, false);
    SetFlagCarrier(otherTeamIdx, player->GetObjectGuid());
    m_flagState[otherTeamIdx] = BG_WS_FLAG_STATE_ON_PLAYER;

    // update world state to show correct flag carrier
    UpdateFlagState(attackerTeam, BG_WS_FLAG_STATE_ON_PLAYER);
    UpdateWorldState(wsStateUpdateId[otherTeamIdx], 1);

    player->CastSpell(player, wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].spellId, TRIGGERED_OLD_TRIGGERED);
    if (m_brutalAssaultActive)
        player->CastSpell(player, BG_WS_SPELL_BRUTAL_ASSAULT, TRIGGERED_OLD_TRIGGERED);
    else if (m_focusedAssaultActive)
        player->CastSpell(player, BG_WS_SPELL_FOCUSED_ASSAULT, TRIGGERED_OLD_TRIGGERED);

    PlaySoundToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].soundId);
    SendMessageToAll(wsgFlagData[otherTeamIdx][BG_WS_FLAG_ACTION_PICKEDUP].messageId, wsgFlagData[teamIdx][BG_WS_FLAG_ACTION_PICKEDUP].chatType, player);
    player->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);

    // start timed achiev
    player->GetAchievementMgr().StartTimedAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, player->GetTeam() == ALLIANCE ? WS_TIMED_ACHIEV_QUICK_CAP_A : WS_TIMED_ACHIEV_QUICK_CAP_H);
}

// Function that handles the click action on the dropped flag
void BattleGroundWS::ProcessDroppedFlagActions(Player* player, GameObject* target)
{
    Team team = player->GetTeam();
    PvpTeamIndex teamIdx = GetTeamIndexByTeamId(team);
    PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(teamIdx);

    int32 actionId = BG_WS_FLAG_ACTION_NONE;

    // check if we are returning our flag
    if (wsDroppedFlagId[teamIdx] == target->GetEntry())
    {
        if (m_flagState[teamIdx] != BG_WS_FLAG_STATE_ON_GROUND)
            return;

        DEBUG_LOG("BattleGroundWS: Team %u has returned the dropped flag %u.", player->GetTeam(), target->GetEntry());

        actionId = BG_WS_FLAG_ACTION_RETURNED;

        UpdateFlagState(GetOtherTeam(team), BG_WS_FLAG_STATE_WAIT_RESPAWN);

        RespawnFlagAtBase(team, false);
        UpdatePlayerScore(player, SCORE_FLAG_RETURNS, 1);

        // send messages and sounds
        SendMessageToAll(wsgFlagData[teamIdx][actionId].messageId, wsgFlagData[teamIdx][actionId].chatType, player);
        PlaySoundToAll(wsgFlagData[teamIdx][actionId].soundId);

        // update achievements
        player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BG_OBJECTIVE_CAPTURE, 44);
    }
    // check if we are picking up enemy flag
    else if (wsDroppedFlagId[otherTeamIdx] == target->GetEntry())
    {
        if (m_flagState[otherTeamIdx] != BG_WS_FLAG_STATE_ON_GROUND)
            return;

        DEBUG_LOG("BattleGroundWS: Team %u has recaptured the dropped flag %u.", player->GetTeam(), target->GetEntry());

        actionId = BG_WS_FLAG_ACTION_PICKEDUP;

        SpawnEvent(otherTeamIdx, 0, false);
        SetFlagCarrier(otherTeamIdx, player->GetObjectGuid());

        player->CastSpell(player, wsgFlagData[otherTeamIdx][actionId].spellId, TRIGGERED_OLD_TRIGGERED);
        if (m_brutalAssaultActive)
            player->CastSpell(player, BG_WS_SPELL_BRUTAL_ASSAULT, TRIGGERED_OLD_TRIGGERED);
        else if (m_focusedAssaultActive)
            player->CastSpell(player, BG_WS_SPELL_FOCUSED_ASSAULT, TRIGGERED_OLD_TRIGGERED);

        m_flagState[otherTeamIdx] = BG_WS_FLAG_STATE_ON_PLAYER;
        UpdateFlagState(team, BG_WS_FLAG_STATE_ON_PLAYER);

        UpdateWorldState(wsStateUpdateId[otherTeamIdx], 1);

        // send messages and sounds
        SendMessageToAll(wsgFlagData[otherTeamIdx][actionId].messageId, wsgFlagData[teamIdx][actionId].chatType, player);
        PlaySoundToAll(wsgFlagData[otherTeamIdx][actionId].soundId);
    }

    if (actionId != BG_WS_FLAG_ACTION_NONE)
        player->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
}

// Handle flag click event
void BattleGroundWS::HandlePlayerClickedOnFlag(Player* player, GameObject* goTarget)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    // assess the current flag state based on the gameobject entry
    switch (goTarget->GetEntry())
    {
        case GO_WS_SILVERWING_FLAG:
        case GO_WS_WARSONG_FLAG:
            ProcessFlagPickUpFromBase(player, player->GetTeam());
            break;
    }
}

// Handle player exit
void BattleGroundWS::RemovePlayer(Player* player, ObjectGuid guid)
{
    std::vector<std::pair<Team, PvpTeamIndex>> checkIndex;
    if (player)
    {
        Team playerTeam = player->GetTeam();
        PvpTeamIndex playerTeamIndex = GetTeamIndexByTeamId(playerTeam);
        PvpTeamIndex otherTeamIdx = GetOtherTeamIndex(playerTeamIndex);
        checkIndex.emplace_back(playerTeam, otherTeamIdx);
    }
    else // if no player, check both
    {
        checkIndex.push_back({ HORDE, TEAM_INDEX_ALLIANCE });
        checkIndex.push_back({ ALLIANCE, TEAM_INDEX_HORDE });
    }

    for (auto& data : checkIndex)
    {
        // Clear flag carrier and respawn main flag
        if (IsFlagPickedUp(data.second) && m_flagCarrier[data.second] == guid)
        {
            if (!player) // recheck the validity of this, shouldnt flag be dropped when player is logged out?
            {
                sLog.outError("BattleGroundWS: Removing offline player who unexpectendly carries the flag!");

                ClearFlagCarrier(data.second);
                RespawnFlagAtBase(data.first, false);
            }
            else
                HandlePlayerDroppedFlag(player);
        }
    }
}

// Update team score world states
void BattleGroundWS::UpdateTeamScore(Team team)
{
    PvpTeamIndex teamIndex = GetTeamIndexByTeamId(team);

    UpdateWorldState(team == ALLIANCE ? BG_WS_STATE_CAPTURES_ALLIANCE : BG_WS_STATE_CAPTURES_HORDE, m_teamScores[teamIndex]);
}

// Handle area trigger
bool BattleGroundWS::HandleAreaTrigger(Player* player, uint32 trigger)
{
    // this is wrong way to implement these things. On official it done by gameobject spell cast.
    if (GetStatus() != STATUS_IN_PROGRESS)
        return false;

    switch (trigger)
    {
        case WS_AT_SILVERWING_ROOM:                                     // Alliance Flag spawn
            if (m_flagState[TEAM_INDEX_HORDE] && !m_flagState[TEAM_INDEX_ALLIANCE])
                if (GetFlagCarrierGuid(TEAM_INDEX_HORDE) == player->GetObjectGuid())
                    ProcessPlayerFlagScoreEvent(player);
            break;
        case WS_AT_WARSONG_ROOM:                                        // Horde Flag spawn
            if (m_flagState[TEAM_INDEX_ALLIANCE] && !m_flagState[TEAM_INDEX_HORDE])
                if (GetFlagCarrierGuid(TEAM_INDEX_ALLIANCE) == player->GetObjectGuid())
                    ProcessPlayerFlagScoreEvent(player);
            break;
        default:
            return false;
    }

    return true;
}

void BattleGroundWS::HandleGameObjectCreate(GameObject* go)
{
    // set initial data and activate capture points
    switch (go->GetEntry())
    {
        case GO_WS_SILVERWING_FLAG_DROP:
            m_droppedFlagGuid[TEAM_INDEX_ALLIANCE] = go->GetObjectGuid();
            break;
        case GO_WS_WARSONG_FLAG_DROP:
            m_droppedFlagGuid[TEAM_INDEX_HORDE] = go->GetObjectGuid();
            break;
    }
}

// process click on dropped flag events
bool BattleGroundWS::HandleEvent(uint32 eventId, GameObject* go, Unit* invoker)
{
    if (invoker->GetTypeId() != TYPEID_PLAYER)
        return true;

    Player* srcPlayer = (Player*)invoker;

    switch (eventId)
    {
        case WS_EVENT_ALLIACE_FLAG_PICKUP:
        case WS_EVENT_HORDE_FLAG_PICKUP:
            ProcessDroppedFlagActions(srcPlayer, go);
            break;
    }

    return false;
}

void BattleGroundWS::Reset()
{
    // call parent's class reset
    BattleGround::Reset();

    // spiritguides and flags not spawned at beginning
    m_activeEvents[WS_EVENT_SPIRITGUIDES_SPAWN] = BG_EVENT_NONE;
    m_activeEvents[WS_EVENT_FLAG_A] = BG_EVENT_NONE;
    m_activeEvents[WS_EVENT_FLAG_H] = BG_EVENT_NONE;

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        m_droppedFlagGuid[i].Clear();
        m_flagCarrier[i].Clear();
        m_flagState[i]      = BG_WS_FLAG_STATE_ON_BASE;
        m_teamScores[i]     = 0;

        m_flagsDropTimer[i] = 0;
        m_flagsTimer[i]     = 0;
    }

    bool isBgWeekend = BattleGroundMgr::IsBgWeekend(GetTypeId());
    m_reputationCapture = (isBgWeekend) ? BG_WS_WEEKEND_FLAG_CAPTURE_REPUTATION : BG_WS_NORMAL_FLAG_CAPTURE_REPUTATION;
    m_honorWinKills = (isBgWeekend) ? BG_WS_WEEKEND_WIN_KILLS : BG_WS_NORMAL_WIN_KILLS;
    m_honorEndKills = (isBgWeekend) ? BG_WS_WEEKEND_MAP_COMPLETE_KILLS : BG_WS_NORMAL_MAP_COMPLETE_KILLS;
    m_flagCarrierDebuffTimer = BG_WS_BRUTAL_ASSAULT_TIME;
    m_brutalAssaultActive = false;
    m_focusedAssaultActive = false;

    // setup graveyards
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_ALLIANCE, BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_MAIN_HORDE, BG_WS_ZONE_ID_MAIN, TEAM_INVALID);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_ALLIANCE, BG_WS_ZONE_ID_MAIN, ALLIANCE);
    GetBgMap()->GetGraveyardManager().SetGraveYardLinkTeam(WS_GRAVEYARD_FLAGROOM_HORDE, BG_WS_ZONE_ID_MAIN, HORDE);

    m_endTimer = BG_WS_TIME_LIMIT;
    m_lastCapturedFlagTeam = TEAM_NONE;
}

void BattleGroundWS::EndBattleGround(Team winner)
{
    // win reward
    RewardHonorToTeam(GetBonusHonorFromKill(m_honorWinKills), winner);

    // complete map_end rewards (even if no team wins)
    RewardHonorToTeam(GetBonusHonorFromKill(m_honorEndKills), ALLIANCE);
    RewardHonorToTeam(GetBonusHonorFromKill(m_honorEndKills), HORDE);

    BattleGround::EndBattleGround(winner);
}

void BattleGroundWS::HandleKillPlayer(Player* player, Player* killer)
{
    if (GetStatus() != STATUS_IN_PROGRESS)
        return;

    HandlePlayerDroppedFlag(player);

    BattleGround::HandleKillPlayer(player, killer);
}

void BattleGroundWS::UpdatePlayerScore(Player* player, uint32 type, uint32 value)
{
    BattleGroundScoreMap::iterator itr = m_playerScores.find(player->GetObjectGuid());
    if (itr == m_playerScores.end())                        // player not found
        return;

    switch (type)
    {
        case SCORE_FLAG_CAPTURES:                           // flags captured
            ((BattleGroundWGScore*)itr->second)->flagCaptures += value;
            break;
        case SCORE_FLAG_RETURNS:                            // flags returned
            ((BattleGroundWGScore*)itr->second)->flagReturns += value;
            break;
        default:
            BattleGround::UpdatePlayerScore(player, type, value);
            break;
    }
}

void BattleGroundWS::FillInitialWorldStates(WorldPacket& data, uint32& count)
{
    FillInitialWorldState(data, count, BG_WS_STATE_CAPTURES_ALLIANCE, m_teamScores[TEAM_INDEX_ALLIANCE]);
    FillInitialWorldState(data, count, BG_WS_STATE_CAPTURES_HORDE, m_teamScores[TEAM_INDEX_HORDE]);

    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        if (m_flagState[i] == BG_WS_FLAG_STATE_ON_GROUND)
            FillInitialWorldState(data, count, wsStateUpdateId[i], -1);
        else if (m_flagState[i] == BG_WS_FLAG_STATE_ON_PLAYER)
            FillInitialWorldState(data, count, wsStateUpdateId[i], 1);
        else
            FillInitialWorldState(data, count, wsStateUpdateId[i], 0);
    }

    FillInitialWorldState(data, count, BG_WS_STATE_CAPTURES_MAX, BG_WS_MAX_TEAM_SCORE);

    FillInitialWorldState(data, count, BG_WS_STATE_FLAG_HORDE, m_flagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_PLAYER ? BG_WS_FLAG_STATE_ON_PLAYER : BG_WS_FLAG_STATE_WAIT_RESPAWN);
    FillInitialWorldState(data, count, BG_WS_STATE_FLAG_ALLIANCE, m_flagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_PLAYER ? BG_WS_FLAG_STATE_ON_PLAYER : BG_WS_FLAG_STATE_WAIT_RESPAWN);

    FillInitialWorldState(data, count, BG_WS_STATE_TIME_ENABLED, WORLD_STATE_ADD);
    FillInitialWorldState(data, count, BG_WS_STATE_TIME_REMAINING, GetRemainingTimeInMinutes());
}

Team BattleGroundWS::GetPrematureWinner()
{
    int32 hordeScore = m_teamScores[TEAM_INDEX_HORDE];
    int32 allianceScore = m_teamScores[TEAM_INDEX_ALLIANCE];

    if (hordeScore > allianceScore)
        return HORDE;
    if (allianceScore > hordeScore)
        return ALLIANCE;

    // If the values are equal, fall back to number of players on each team
    return BattleGround::GetPrematureWinner();
}

bool BattleGroundWS::CheckAchievementCriteriaMeet(uint32 criteria_id, Player const* source, Unit const* target, uint32 miscvalue1)
{
    switch (criteria_id)
    {
        case WS_ACHIEV_SAVE_THE_DAY_1:
        case WS_ACHIEV_SAVE_THE_DAY_2:
            return (source->GetTeam() == HORDE && source->GetAreaId() == 4571 && target->HasAura(BG_WS_SPELL_WARSONG_FLAG) && m_flagState[TEAM_INDEX_ALLIANCE] == BG_WS_FLAG_STATE_ON_BASE) ||
                (source->GetTeam() == ALLIANCE && source->GetAreaId() == 4572 && target->HasAura(BG_WS_SPELL_SILVERWING_FLAG) && m_flagState[TEAM_INDEX_HORDE] == BG_WS_FLAG_STATE_ON_BASE);
    }

    return false;
}