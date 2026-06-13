/*
 * mod-xp-dynamic
 *
 * Credits:
 * Script reworked by Micrah/Milestorme and Poszer
 * Module Created by Micrah/Milestorme
 * Original Script from AshmaneCore https://github.com/conan513 Single Player Project
 * Extended with Gold, Reputation, Alt Progression Bonus, Skill XP, Class Fixes
 */

#include "Chat.h"
#include "Configuration/Config.h"
#include "Player.h"
#include "Creature.h"
#include "ScriptMgr.h"
#include "Map.h"
#include "Group.h"
#include "DBCStores.h"
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

    // ── Skill XP Roll ─────────────────────────────────────────────────
    // 1 pt = 35%, 2 pts = 25%, 3 pts = 20%, 4 pts = 13%, 5 pts = 7%

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

        // Overflow: lucky rolls can push past the skill cap by up to 5 points
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
        case 1: break; // Silent
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

            // Announce jackpot to group
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

    // ── Account Tracking ─────────────────────────────────────────────

    static uint8 GetAccountHighestLevel(uint32 accountId)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT highest_level FROM account_highest_level WHERE account_id = {}",
            accountId);
        return result ? result->Fetch()[0].Get<uint8>() : 0;
    }

    // Rebuild highest level by scanning ALL characters on the account.
    // Used on login to self-heal wiped or missing table entries.
    static void RebuildAccountHighestLevel(uint32 accountId, uint8 currentPlayerLevel)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT MAX(level) FROM characters WHERE account = {}",
            accountId);

        uint8 trueHighest = result ? result->Fetch()[0].Get<uint8>() : currentPlayerLevel;

        // Ensure current player's level is included even if not saved to DB yet
        if (currentPlayerLevel > trueHighest)
            trueHighest = currentPlayerLevel;

        CharacterDatabase.Execute(
            "INSERT INTO account_highest_level (account_id, highest_level) VALUES ({}, {}) "
            "ON DUPLICATE KEY UPDATE highest_level = {}",
            accountId, trueHighest, trueHighest);
    }

    static void UpdateAccountHighestLevel(uint32 accountId, uint8 newLevel)
    {
        CharacterDatabase.Execute(
            "INSERT INTO account_highest_level (account_id, highest_level) VALUES ({}, {}) "
            "ON DUPLICATE KEY UPDATE highest_level = GREATEST(highest_level, {})",
            accountId, newLevel, newLevel);
    }

    static float GetCachedAltMultiplier(uint32 accountId)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT multiplier FROM account_xp_bonus WHERE account_id = {}",
            accountId);
        return result ? result->Fetch()[0].Get<float>() : 1.0f;
    }

    // Recalculate alt bonus based on number of server-capped characters.
    // Pass currentPlayer when calling from OnPlayerLevelChanged at cap —
    // their new level may not be written to the characters table yet,
    // so we exclude them from the DB query and add 1 manually.
    static void RecalculateAltBonus(uint32 accountId, Player* currentPlayer = nullptr)
    {
        uint8 serverCap = static_cast<uint8>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));
        float perChar = sConfigMgr->GetOption<float>("Dynamic.XP.AccountBonus.PerMaxedChar", 0.25f);
        float maxMult = sConfigMgr->GetOption<float>("Dynamic.XP.AccountBonus.MaxMultiplier", 5.0f);

        uint32 cappedCount = 0;

        if (currentPlayer)
        {
            // Exclude current player from DB query — level not saved yet
            QueryResult result = CharacterDatabase.Query(
                "SELECT COUNT(*) FROM characters WHERE account = {} AND level >= {} AND guid != {}",
                accountId, serverCap, currentPlayer->GetGUID().GetCounter());

            cappedCount = (result ? result->Fetch()[0].Get<uint32>() : 0) + 1;
        }
        else
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT COUNT(*) FROM characters WHERE account = {} AND level >= {}",
                accountId, serverCap);

            cappedCount = result ? result->Fetch()[0].Get<uint32>() : 0;
        }

        float multiplier = std::min(1.0f + (cappedCount * perChar), maxMult);

        CharacterDatabase.Execute(
            "INSERT INTO account_xp_bonus (account_id, capped_chars, multiplier) "
            "VALUES ({}, {}, {}) "
            "ON DUPLICATE KEY UPDATE capped_chars = {}, multiplier = {}",
            accountId, cappedCount, multiplier, cappedCount, multiplier);
    }

    // ── Alt Bonus Logic ───────────────────────────────────────────────
    // Returns the XP multiplier for an alt character.
    // Bonus applies only when the player is below their account's personal
    // highest level. Magnitude is based on number of server-capped chars.

    float GetAltBonus(Player* player)
    {
        if (!sConfigMgr->GetOption<bool>("Dynamic.XP.AccountBonus.Enable", true))
            return 1.0f;

        uint8  playerLevel = player->GetLevel();
        uint32 accountId = player->GetSession()->GetAccountId();
        uint8  highestLevel = GetAccountHighestLevel(accountId);

        // No bonus if no higher character exists on the account
        if (highestLevel <= 1)
            return 1.0f;

        // No bonus at or above the account's personal highest level
        if (playerLevel >= highestLevel)
            return 1.0f;

        return GetCachedAltMultiplier(accountId);
    }

    // ── Hooks ─────────────────────────────────────────────────────────

    void OnPlayerLogin(Player* player) override
    {
        if (sConfigMgr->GetOption<bool>("Dynamic.XP.Rate.Announce", true))
            ChatHandler(player->GetSession()).SendSysMessage(
                "This server is running the |cff4CFF00Dynamic Rate|r module.");

        uint32 accountId = player->GetSession()->GetAccountId();

        // Self-heal on login: scan ALL characters on the account to rebuild
        // the true highest level. Handles wiped tables, new installs, and
        // the playerbot scenario where multiple chars level simultaneously.
        RebuildAccountHighestLevel(accountId, player->GetLevel());

        // Recalculate alt bonus from actual capped char count in DB
        RecalculateAltBonus(accountId);
    }

    void OnPlayerLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        uint32 accountId = player->GetSession()->GetAccountId();
        uint8  newLevel = player->GetLevel();
        uint8  serverCap = static_cast<uint8>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));

        // Update highest level for this account
        // Handles both real players and playerbots leveling in the same group
        UpdateAccountHighestLevel(accountId, newLevel);

        // Recalculate alt bonus multiplier
        // At server cap: exclude current player from DB query (timing fix)
        // Below cap: normal DB query is fine
        if (newLevel >= serverCap)
            RecalculateAltBonus(accountId, player);
        else
            RecalculateAltBonus(accountId);

        // ── Class Fixes ───────────────────────────────────────────────
        // Grant spells that cross-faction ARAC characters cannot obtain
        // through normal trainer/quest chains.

        uint8  playerClass = player->getClass();
        TeamId team = player->GetTeamId();

        // Horde Paladin — Redemption at level 12 (Alliance trainer timing)
        if (sConfigMgr->GetOption<bool>("Dynamic.ClassFix.HordePaladin.Enable", true))
            if (playerClass == CLASS_PALADIN && team == TEAM_HORDE && newLevel == 12)
                player->learnSpell(7328);

        // Alliance Shaman — Totem unlock spells at Horde quest equivalent levels
        if (sConfigMgr->GetOption<bool>("Dynamic.ClassFix.AllianceShaman.Enable", true))
        {
            if (playerClass == CLASS_SHAMAN && team == TEAM_ALLIANCE)
            {
                switch (newLevel)
                {
                case 4:  player->learnSpell(8073); break; // Earth Totem
                case 10: player->learnSpell(2075); break; // Fire Totem
                case 20: player->learnSpell(5396); break; // Water Totem
                    // Air Totem (8385): no spell needed, totem item is sufficient
                default: break;
                }
            }
        }
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 xpSource) override
    {
        if (!player || !sConfigMgr->GetOption<bool>("Dynamic.XP.Rate", true))
            return;

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

        amount = static_cast<uint32>(std::round(amount * bracket * source * altBonus * botRate));
    }

    bool OnPlayerReputationChange(Player* player, uint32 /*factionID*/, int32& standing, bool /*incremental*/) override
    {
        if (!sConfigMgr->GetOption<bool>("Dynamic.Rep.Rate", true))
            return true;

        float rate = GetBracketRate(player->GetLevel(), "Dynamic.Rep.Rate");
        float altBonus = sConfigMgr->GetOption<bool>("Dynamic.Rep.AccountBonus.Enable", false)
            ? GetAltBonus(player) : 1.0f;

        standing = static_cast<int32>(std::round(standing * rate * altBonus));
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

        killed->loot.gold = static_cast<uint32>(std::round(killed->loot.gold * rate));
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
        AwardSkillXP(player, skillId, true); // overflow allowed
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
        AwardSkillXP(player, skill->SkillLine, true); // overflow allowed
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
        AwardSkillXP(player, skillId, false); // no overflow for combat skills
    }
};

void AddSC_dynamic_xp_rate()
{
    new spp_dynamic_xp_rate();
}
