/*
 * mod-xp-dynamic
 *
 * Credits:
 * Script reworked by Micrah/Milestorme and Poszer
 * Module Created by Micrah/Milestorme
 * Original Script from AshmaneCore https://github.com/conan513 Single Player Project
 * Extended with Gold, Reputation, Alt Progression Bonus, Skill XP, Class Fixes,
 * Account Linking, Faction-Gated Alt Bonus
 */

#include "Chat.h"
#include "Configuration/Config.h"
#include "Player.h"
#include "Creature.h"
#include "ScriptMgr.h"
#include "Map.h"
#include "Group.h"
#include "DBCStores.h"
#include "StringFormat.h"
#include <cmath>

class spp_dynamic_xp_rate : public PlayerScript
{
public:
    spp_dynamic_xp_rate() : PlayerScript("spp_dynamic_xp_rate", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_GIVE_EXP,
        PLAYERHOOK_ON_REPUTATION_CHANGE,
        PLAYERHOOK_ON_CREATURE_KILL,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_UPDATE_GATHERING_SKILL,
        PLAYERHOOK_ON_UPDATE_CRAFTING_SKILL,
        PLAYERHOOK_ON_UPDATE_SKILL
        }) {}

    // ── Helpers ───────────────────────────────────────────────────────

    static float GetBracketRate(uint8 level, const std::string& prefix)
    {
        if (level <= 4)       return sConfigMgr->GetOption<float>(prefix + ".1-4", 1.0f);
        else if (level <= 9)  return sConfigMgr->GetOption<float>(prefix + ".5-9", 1.0f);
        else if (level <= 14) return sConfigMgr->GetOption<float>(prefix + ".10-14", 1.0f);
        else if (level <= 19) return sConfigMgr->GetOption<float>(prefix + ".15-19", 1.0f);
        else if (level <= 24) return sConfigMgr->GetOption<float>(prefix + ".20-24", 1.0f);
        else if (level <= 29) return sConfigMgr->GetOption<float>(prefix + ".25-29", 1.0f);
        else if (level <= 34) return sConfigMgr->GetOption<float>(prefix + ".30-34", 1.0f);
        else if (level <= 39) return sConfigMgr->GetOption<float>(prefix + ".35-39", 1.0f);
        else if (level <= 44) return sConfigMgr->GetOption<float>(prefix + ".40-44", 1.0f);
        else if (level <= 49) return sConfigMgr->GetOption<float>(prefix + ".45-49", 1.0f);
        else if (level <= 54) return sConfigMgr->GetOption<float>(prefix + ".50-54", 1.0f);
        else if (level <= 59) return sConfigMgr->GetOption<float>(prefix + ".55-59", 1.0f);
        else if (level <= 64) return sConfigMgr->GetOption<float>(prefix + ".60-64", 1.0f);
        else if (level <= 69) return sConfigMgr->GetOption<float>(prefix + ".65-69", 1.0f);
        else if (level <= 74) return sConfigMgr->GetOption<float>(prefix + ".70-74", 1.0f);
        else                  return sConfigMgr->GetOption<float>(prefix + ".75-79", 1.0f);
    }

    static bool IsInInstance(Player* player)
    {
        Map* map = player->GetMap();
        return map && (map->IsDungeon() || map->IsRaid());
    }

    static bool IsPlayerBot(Player* player)
    {
        return player->GetSession() && player->GetSession()->IsBot();
    }

    static bool BotHasRealPlayerInGroup(Player* player)
    {
        Group* group = player->GetGroup();
        if (!group)
            return false;

        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (member && member != player && !IsPlayerBot(member))
                return true;
        }
        return false;
    }

    // ── Faction Helpers ───────────────────────────────────────────────
    // faction: 0 = Alliance, 1 = Horde
    // Alliance races: 1 Human, 3 Dwarf, 4 NightElf, 7 Gnome, 11 Draenei
    // Horde races:    2 Orc,   5 Undead, 6 Tauren,  8 Troll, 10 BloodElf

    static uint8 GetFactionId(TeamId team)
    {
        return (team == TEAM_ALLIANCE) ? 0 : 1;
    }

    static bool IsAllianceRace(uint8 race)
    {
        return race == 1 || race == 3 || race == 4 || race == 7 || race == 11;
    }

    // ── Account Linking ───────────────────────────────────────────────

    static uint32 GetAccountGroupId(uint32 accountId)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT group_id FROM account_link_groups WHERE account_id = {}",
            accountId);
        return result ? result->Fetch()[0].Get<uint32>() : 0;
    }

    // Returns true if this account is linked to others
    static bool IsAccountLinked(uint32 accountId)
    {
        return GetAccountGroupId(accountId) > 0;
    }

    // ── Skill XP Roll ─────────────────────────────────────────────────

    static uint32 GetSkillXPRoll()
    {
        uint32 roll = urand(1, 100);
        if (roll <= 35)      return 1;
        else if (roll <= 60) return 2;
        else if (roll <= 80) return 3;
        else if (roll <= 93) return 4;
        else                 return 5;
    }

    void AwardSkillXP(Player* player, uint32 skillId, bool allowOverflow = false)
    {
        uint32 xp = GetSkillXPRoll();

        if (allowOverflow && xp > 1)
        {
            uint32 current = player->GetSkillValue(skillId);
            uint32 max = player->GetMaxSkillValue(skillId);

            if (current >= max && current < max + 5)
            {
                uint32 newVal = std::min(current + xp, max + 5u);
                player->SetSkill(skillId, player->GetSkillStep(skillId), newVal, newVal);
            }
        }

        player->GiveXP(xp, nullptr);

        switch (xp)
        {
        case 1: break;
        case 2:
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ff00Lucky! Bonus skill gain! +2 XP!|r");
            break;
        case 3:
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cff00ccffNice roll! Bonus skill gain! +3 XP!|r");
            break;
        case 4:
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffff9900Hot streak! Bonus skill gain! +4 XP!|r");
            break;
        case 5:
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffFFD700*** JACKPOT! Bonus skill gain! +5 XP! ***|r");

            if (Group* group = player->GetGroup())
                for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
                    if (Player* member = gref->GetSource(); member && member != player)
                        ChatHandler(member->GetSession()).PSendSysMessage(
                            "|cffFFD700*** %s hit a JACKPOT skill roll! ***|r",
                            player->GetName().c_str());
            break;
        }
        default: break;
        }
    }

    // ── Account Tracking (Faction-Aware) ─────────────────────────────

    static uint8 GetAccountHighestLevel(uint32 accountId, uint8 faction)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT highest_level FROM account_highest_level "
            "WHERE account_id = {} AND faction = {}",
            accountId, faction);
        return result ? result->Fetch()[0].Get<uint8>() : 0;
    }

    static void RebuildAccountHighestLevel(uint32 accountId, uint8 faction, uint8 currentPlayerLevel)
    {
        // Scan all chars on account for this faction
        QueryResult result;
        if (faction == 0) // Alliance
            result = CharacterDatabase.Query(
                "SELECT MAX(level) FROM characters WHERE account = {} AND race IN (1,3,4,7,11)",
                accountId);
        else // Horde
            result = CharacterDatabase.Query(
                "SELECT MAX(level) FROM characters WHERE account = {} AND race IN (2,5,6,8,10)",
                accountId);

        uint8 trueHighest = result ? result->Fetch()[0].Get<uint8>() : currentPlayerLevel;
        if (currentPlayerLevel > trueHighest)
            trueHighest = currentPlayerLevel;

        CharacterDatabase.Execute(
            "INSERT INTO account_highest_level (account_id, faction, highest_level) VALUES ({}, {}, {}) "
            "ON DUPLICATE KEY UPDATE highest_level = {}",
            accountId, faction, trueHighest, trueHighest);
    }

    static void UpdateAccountHighestLevel(uint32 accountId, uint8 faction, uint8 newLevel)
    {
        CharacterDatabase.Execute(
            "INSERT INTO account_highest_level (account_id, faction, highest_level) VALUES ({}, {}, {}) "
            "ON DUPLICATE KEY UPDATE highest_level = GREATEST(highest_level, {})",
            accountId, faction, newLevel, newLevel);
    }

    static float GetCachedAltMultiplier(uint32 accountId, uint8 faction)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT multiplier FROM account_xp_bonus WHERE account_id = {} AND faction = {}",
            accountId, faction);
        return result ? result->Fetch()[0].Get<float>() : 1.0f;
    }

    // ── Alt Bonus Recalculation (Faction + Account Link Aware) ────────
    // Counts chars AT the highest level for this faction.
    // If accounts are linked, counts across ALL linked accounts.
    // Faction gate ensures Alliance bonus only comes from Alliance chars
    // and Horde bonus only from Horde chars.

    static void RecalculateAltBonus(uint32 accountId, uint8 faction, Player* currentPlayer = nullptr)
    {
        float perChar = sConfigMgr->GetOption<float>("Dynamic.XP.AccountBonus.PerMaxedChar", 0.25f);
        float maxMult = sConfigMgr->GetOption<float>("Dynamic.XP.AccountBonus.MaxMultiplier", 5.0f);

        uint8 highestLevel = GetAccountHighestLevel(accountId, faction);
        if (highestLevel <= 1)
            return;

        uint32 groupId = GetAccountGroupId(accountId);
        uint32 charsAtHighest = 0;

        // Build race filter string based on faction
        const char* raceFilter = (faction == 0) ? "1,3,4,7,11" : "2,5,6,8,10";

        if (currentPlayer && currentPlayer->GetLevel() == highestLevel)
        {
            // Timing gap fix — player not saved to DB yet, count others + 1
            if (groupId > 0)
            {
                QueryResult result = CharacterDatabase.Query(
                    "SELECT COUNT(*) FROM characters WHERE account IN "
                    "(SELECT account_id FROM account_link_groups WHERE group_id = {}) "
                    "AND level = {} AND guid != {} AND race IN ({})",
                    groupId, highestLevel, currentPlayer->GetGUID().GetCounter(), raceFilter);
                charsAtHighest = (result ? result->Fetch()[0].Get<uint32>() : 0) + 1;
            }
            else
            {
                QueryResult result = CharacterDatabase.Query(
                    "SELECT COUNT(*) FROM characters WHERE account = {} "
                    "AND level = {} AND guid != {} AND race IN ({})",
                    accountId, highestLevel, currentPlayer->GetGUID().GetCounter(), raceFilter);
                charsAtHighest = (result ? result->Fetch()[0].Get<uint32>() : 0) + 1;
            }
        }
        else
        {
            if (groupId > 0)
            {
                QueryResult result = CharacterDatabase.Query(
                    "SELECT COUNT(*) FROM characters WHERE account IN "
                    "(SELECT account_id FROM account_link_groups WHERE group_id = {}) "
                    "AND level = {} AND race IN ({})",
                    groupId, highestLevel, raceFilter);
                charsAtHighest = result ? result->Fetch()[0].Get<uint32>() : 0;
            }
            else
            {
                QueryResult result = CharacterDatabase.Query(
                    "SELECT COUNT(*) FROM characters WHERE account = {} "
                    "AND level = {} AND race IN ({})",
                    accountId, highestLevel, raceFilter);
                charsAtHighest = result ? result->Fetch()[0].Get<uint32>() : 0;
            }
        }

        float multiplier = std::min(1.0f + (charsAtHighest * perChar), maxMult);

        CharacterDatabase.Execute(
            "INSERT INTO account_xp_bonus (account_id, faction, capped_chars, multiplier) "
            "VALUES ({}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE capped_chars = {}, multiplier = {}",
            accountId, faction, charsAtHighest, multiplier, charsAtHighest, multiplier);
    }

    // ── Alt Bonus Gate ────────────────────────────────────────────────
    // Faction-aware: Alliance chars only get bonus from Alliance leaders,
    // Horde chars only from Horde leaders.
    // Bonus only applies when player is below their faction's highest level.

    float GetAltBonus(Player* player)
    {
        if (!sConfigMgr->GetOption<bool>("Dynamic.XP.AccountBonus.Enable", true))
            return 1.0f;

        uint8  playerLevel = player->GetLevel();
        uint32 accountId = player->GetSession()->GetAccountId();
        uint8  faction = GetFactionId(player->GetTeamId());
        uint8  highestLevel = GetAccountHighestLevel(accountId, faction);

        if (highestLevel <= 1)
            return 1.0f;

        if (playerLevel >= highestLevel)
            return 1.0f;

        return GetCachedAltMultiplier(accountId, faction);
    }

    // ── Hooks ─────────────────────────────────────────────────────────

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("Dynamic.XP.Rate.Announce", true))
            ChatHandler(player->GetSession()).SendSysMessage(
                "This server is running the |cff4CFF00Dynamic Rate|r module.");

        uint32 accountId = player->GetSession()->GetAccountId();
        uint8  faction = GetFactionId(player->GetTeamId());

        RebuildAccountHighestLevel(accountId, faction, player->GetLevel());
        RecalculateAltBonus(accountId, faction);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        uint8  newLevel = player->GetLevel();
        uint8  faction = GetFactionId(player->GetTeamId());

        UpdateAccountHighestLevel(accountId, faction, newLevel);
        RecalculateAltBonus(accountId, faction, player);

        // ── Class Fixes ───────────────────────────────────────────────

        uint8  playerClass = player->getClass();
        TeamId team = player->GetTeamId();

        if (sConfigMgr->GetOption<bool>("Dynamic.ClassFix.HordePaladin.Enable", true))
            if (playerClass == CLASS_PALADIN && team == TEAM_HORDE && newLevel == 12)
                player->learnSpell(7328);

        if (sConfigMgr->GetOption<bool>("Dynamic.ClassFix.AllianceShaman.Enable", true))
        {
            if (playerClass == CLASS_SHAMAN && team == TEAM_ALLIANCE)
            {
                switch (newLevel)
                {
                case 4:  player->learnSpell(8073); break;
                case 10: player->learnSpell(2075); break;
                case 20: player->learnSpell(5396); break;
                default: break;
                }
            }
        }
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 xpSource) override
    {
        if (!player || !sConfigMgr->GetOption<bool>("Dynamic.XP.Rate", true))
            return;

        uint32 originalAmount = amount;

        uint8 level = player->GetLevel();
        bool inInstance = IsInInstance(player);

        float bracket = GetBracketRate(level, "Dynamic.XP.Rate");

        float source = 1.0f;
        if (xpSource == 0)
            source = inInstance
            ? sConfigMgr->GetOption<float>("Dynamic.XP.Rate.Kill.Dungeon", 3.0f)
            : sConfigMgr->GetOption<float>("Dynamic.XP.Rate.Kill.World", 1.0f);
        else if (xpSource == 1)
            source = inInstance
            ? sConfigMgr->GetOption<float>("Dynamic.XP.Rate.Quest.Dungeon", 3.5f)
            : sConfigMgr->GetOption<float>("Dynamic.XP.Rate.Quest.World", 2.0f);
        else if (xpSource == 2)
            source = sConfigMgr->GetOption<float>("Dynamic.XP.Rate.Explore", 1.5f);

        float altBonus = GetAltBonus(player);

        float botRate = 1.0f;
        if (IsPlayerBot(player) && !BotHasRealPlayerInGroup(player))
            botRate = sConfigMgr->GetOption<float>("Dynamic.XP.BotSoloRate", 0.5f);

        float totalMult = bracket * source * altBonus * botRate;
        amount = static_cast<uint32>(std::round(originalAmount * totalMult));

        // Show XP breakdown to real players when rate changes the value
        if (!IsPlayerBot(player) && amount != originalAmount)
        {
            std::string msg = Acore::StringFormat(
                "|cff00CCFFAshbringer XP:|r {} |cff888888→|r |cff00FF00{}|r |cff888888(|r|cffFFD700{:.2f}x|r|cff888888)|r",
                originalAmount, amount, totalMult);
            ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());
        }
    }

    bool OnPlayerReputationChange(Player* player, uint32 /*factionID*/, int32& standing, bool /*incremental*/) override
    {
        if (!sConfigMgr->GetOption<bool>("Dynamic.Rep.Rate", true))
            return true;

        if (standing <= 0)
            return true; // don't message on rep loss

        int32 originalStanding = standing;

        float rate = GetBracketRate(player->GetLevel(), "Dynamic.Rep.Rate");
        float altBonus = sConfigMgr->GetOption<bool>("Dynamic.Rep.AccountBonus.Enable", false)
            ? GetAltBonus(player) : 1.0f;

        float totalMult = rate * altBonus;
        standing = static_cast<int32>(std::round(standing * totalMult));

        // Show rep breakdown to real players when rate changes the value
        if (!IsPlayerBot(player) && standing != originalStanding)
        {
            std::string msg = Acore::StringFormat(
                "|cffFF8C00Ashbringer REP:|r {} |cff888888→|r |cffFFCC00{}|r |cff888888(|r|cffFFD700{:.2f}x|r|cff888888)|r",
                originalStanding, standing, totalMult);
            ChatHandler(player->GetSession()).SendSysMessage(msg.c_str());
        }

        return true;
    }

    void OnPlayerCreatureKill(Player* killer, Creature* killed) override
    {
        if (!killer || !killed || killed->loot.gold == 0)
            return;

        if (!sConfigMgr->GetOption<bool>("Dynamic.Gold.Rate", true))
            return;

        float rate = GetBracketRate(killer->GetLevel(), "Dynamic.Gold.Rate");
        if (rate == 1.0f)
            return;

        uint32 originalGold = killed->loot.gold;
        killed->loot.gold = static_cast<uint32>(std::round(killed->loot.gold * rate));

        // Show gold boost to real players
        if (!IsPlayerBot(killer) && killed->loot.gold != originalGold)
        {
            std::string msg = Acore::StringFormat(
                "|cffFFD700Ashbringer GOLD:|r {}c |cff888888→|r |cffFFFF00{}c|r |cff888888(|r|cffFFD700{:.2f}x|r|cff888888)|r",
                originalGold, killed->loot.gold, rate);
            ChatHandler(killer->GetSession()).SendSysMessage(msg.c_str());
        }
    }

    // Gathering — herbalism, mining, skinning
    void OnPlayerUpdateGatheringSkill(Player* player, uint32 skillId, uint32 /*currentLevel*/,
        uint32 /*gray*/, uint32 /*green*/, uint32 /*yellow*/, uint32& gain) override
    {
        if (!player || gain == 0)
            return;
        if (!sConfigMgr->GetOption<bool>("Dynamic.Skill.XP.Enable", true))
            return;
        if (IsPlayerBot(player) && !BotHasRealPlayerInGroup(player))
            return;
        AwardSkillXP(player, skillId, true);
    }

    // Crafting — alchemy, blacksmithing, cooking, etc.
    void OnPlayerUpdateCraftingSkill(Player* player, SkillLineAbilityEntry const* skill,
        uint32 /*currentLevel*/, uint32& gain) override
    {
        if (!player || gain == 0)
            return;
        if (!sConfigMgr->GetOption<bool>("Dynamic.Skill.XP.Enable", true))
            return;
        if (IsPlayerBot(player) && !BotHasRealPlayerInGroup(player))
            return;
        AwardSkillXP(player, skill->SkillLine, true);
    }

    // Weapons, defense, fishing, etc.
    void OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 value,
        uint32 /*max*/, uint32 /*step*/, uint32 newValue) override
    {
        if (!player || newValue <= value)
            return;
        if (!sConfigMgr->GetOption<bool>("Dynamic.Skill.XP.Enable", true))
            return;
        if (IsPlayerBot(player) && !BotHasRealPlayerInGroup(player))
            return;
        AwardSkillXP(player, skillId, false);
    }
};

void AddSC_dynamic_xp_rate()
{
    new spp_dynamic_xp_rate();
}
