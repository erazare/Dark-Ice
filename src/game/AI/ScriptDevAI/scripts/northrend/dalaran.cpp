/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

/* ScriptData
SDName: Dalaran
SD%Complete: 100
SDComment:
SDCategory: Dalaran
EndScriptData */

/* ContentData
npc_dalaran_guardian_mage
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "Spells/Scripts/SpellScript.h"
#include "Spells/SpellAuras.h"

enum
{
    SPELL_TRESPASSER_H          = 54029,
    SPELL_TRESPASSER_A          = 54028,

    // Exception auras - used for quests 20439 and 24451
    SPELL_COVENANT_DISGUISE_1   = 70971,
    SPELL_COVENANT_DISGUISE_2   = 70972,
    SPELL_SUNREAVER_DISGUISE_1  = 70973,
    SPELL_SUNREAVER_DISGUISE_2  = 70974,

    AREA_ID_SUNREAVER           = 4616,
    AREA_ID_SILVER_ENCLAVE      = 4740
};

struct npc_dalaran_guardian_mageAI : public ScriptedAI
{
    npc_dalaran_guardian_mageAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    void MoveInLineOfSight(Unit* who) override
    {
        if (m_creature->GetDistanceZ(who) > CREATURE_Z_ATTACK_RANGE_MELEE)
            return;

        if (m_creature->IsEnemy(who))
        {
            // exception for quests 20439 and 24451
            if (who->HasAura(SPELL_COVENANT_DISGUISE_1) || who->HasAura(SPELL_COVENANT_DISGUISE_2) ||
                    who->HasAura(SPELL_SUNREAVER_DISGUISE_1) || who->HasAura(SPELL_SUNREAVER_DISGUISE_2))
                return;

            if (m_creature->IsWithinDistInMap(who, m_creature->GetAttackDistance(who)) && m_creature->IsWithinLOSInMap(who))
            {
                if (Player* pPlayer = who->GetBeneficiaryPlayer())
                {
                    // it's mentioned that pet may also be teleported, if so, we need to tune script to apply to those in addition.

                    if (pPlayer->GetAreaId() == AREA_ID_SILVER_ENCLAVE)
                        DoCastSpellIfCan(pPlayer, SPELL_TRESPASSER_A);
                    else if (pPlayer->GetAreaId() == AREA_ID_SUNREAVER)
                        DoCastSpellIfCan(pPlayer, SPELL_TRESPASSER_H);
                }
            }
        }
    }

    void AttackedBy(Unit* /*pAttacker*/) override {}

    void Reset() override {}

    void UpdateAI(const uint32 /*uiDiff*/) override {}
};

/*######
## spell_teleporting_dalaran - 59317
######*/

struct spell_teleporting_dalaran : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* target = spell->GetUnitTarget();
        if (!target || !target->IsPlayer())
            return;

        Player* playerTarget = static_cast<Player*>(target);

        // return from top
        if (playerTarget->GetAreaId() == 4637)
            target->CastSpell(target, 59316, TRIGGERED_OLD_TRIGGERED);
        // teleport atop
        else
            target->CastSpell(target, 59314, TRIGGERED_OLD_TRIGGERED);
    }
};

void AddSC_dalaran()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_dalaran_guardian_mage";
    pNewScript->GetAI = &GetNewAIInstance<npc_dalaran_guardian_mageAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<spell_teleporting_dalaran>("spell_teleporting_dalaran");
}
