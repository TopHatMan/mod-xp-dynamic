#include "Chat.h"
#include "ChatCommand.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "StringFormat.h"
#include <sstream>
#include <vector>

using namespace Acore::ChatCommands;

class dynxp_commandscript : public CommandScript
{
public:
    dynxp_commandscript() : CommandScript("dynxp_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable dynxpCommandTable =
        {
            { "account_link",   HandleAccountLink,   SEC_GAMEMASTER, Console::Yes },  // GM only
            { "account_unlink", HandleAccountUnlink, SEC_GAMEMASTER, Console::Yes },  // GM only
            { "account_info",   HandleAccountInfo,   SEC_PLAYER,     Console::No  },  // anyone
        };

        static ChatCommandTable commandTable =
        {
            { "dynxp", dynxpCommandTable },
        };

        return commandTable;
    }

    static bool HandleAccountLink(ChatHandler* handler, std::string const& args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("Usage: dynxp account_link <id1,id2,id3,...>");
            return false;
        }

        std::vector<uint32> accountIds;
        std::stringstream ss(args);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            if (token.empty()) continue;
            try { accountIds.push_back(static_cast<uint32>(std::stoul(token))); }
            catch (...) { handler->PSendSysMessage("Invalid account ID: '{}'", token); return false; }
        }

        if (accountIds.size() < 2)
        {
            handler->SendSysMessage("Need at least 2 account IDs to link.");
            return false;
        }

        // Find existing group or create new one
        uint32 groupId = 0;
        for (uint32 id : accountIds)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT group_id FROM account_link_groups WHERE account_id = {}", id);
            if (result) { groupId = result->Fetch()[0].Get<uint32>(); break; }
        }

        if (groupId == 0)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT MAX(group_id) FROM account_link_groups");
            groupId = result ? (result->Fetch()[0].Get<uint32>() + 1) : 1;
        }

        for (uint32 id : accountIds)
        {
            CharacterDatabase.Execute(
                "INSERT INTO account_link_groups (account_id, group_id) VALUES ({}, {}) "
                "ON DUPLICATE KEY UPDATE group_id = {}",
                id, groupId, groupId);
        }

        handler->PSendSysMessage("|cff00FF00Linked {} accounts under group {}:|r",
            accountIds.size(), groupId);
        for (uint32 id : accountIds)
            handler->PSendSysMessage("  -> Account {}", id);

        return true;
    }

    static bool HandleAccountUnlink(ChatHandler* handler, std::string const& args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("Usage: dynxp account_unlink <account_id>");
            return false;
        }

        uint32 accountId = static_cast<uint32>(std::stoul(args));

        QueryResult result = CharacterDatabase.Query(
            "SELECT group_id FROM account_link_groups WHERE account_id = {}", accountId);

        if (!result)
        {
            handler->PSendSysMessage("Account {} is not in any link group.", accountId);
            return false;
        }

        uint32 groupId = result->Fetch()[0].Get<uint32>();
        CharacterDatabase.Execute(
            "DELETE FROM account_link_groups WHERE account_id = {}", accountId);

        handler->PSendSysMessage("|cffFFD700Account {} removed from group {}.|r",
            accountId, groupId);
        return true;
    }

    static bool HandleAccountInfo(ChatHandler* handler, std::string const& args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("Usage: dynxp account_info <account_id>");
            return false;
        }

        uint32 accountId = static_cast<uint32>(std::stoul(args));

        QueryResult groupResult = CharacterDatabase.Query(
            "SELECT group_id FROM account_link_groups WHERE account_id = {}", accountId);

        if (groupResult)
        {
            uint32 groupId = groupResult->Fetch()[0].Get<uint32>();
            handler->PSendSysMessage("Account {} is in link group {}.", accountId, groupId);

            QueryResult members = CharacterDatabase.Query(
                "SELECT account_id FROM account_link_groups WHERE group_id = {}", groupId);
            if (members)
            {
                handler->SendSysMessage("Group members:");
                do {
                    handler->PSendSysMessage("  -> Account {}",
                        members->Fetch()[0].Get<uint32>());
                } while (members->NextRow());
            }
        }
        else
        {
            handler->PSendSysMessage("Account {} has no link group.", accountId);
        }

        for (uint8 faction = 0; faction <= 1; ++faction)
        {
            const char* factionName = (faction == 0) ? "Alliance" : "Horde";

            QueryResult hlResult = CharacterDatabase.Query(
                "SELECT highest_level FROM account_highest_level "
                "WHERE account_id = {} AND faction = {}", accountId, faction);

            QueryResult bonusResult = CharacterDatabase.Query(
                "SELECT capped_chars, multiplier FROM account_xp_bonus "
                "WHERE account_id = {} AND faction = {}", accountId, faction);

            uint8  highestLevel = hlResult ? hlResult->Fetch()[0].Get<uint8>() : 0;
            uint32 cappedChars = bonusResult ? bonusResult->Fetch()[0].Get<uint32>() : 0;
            float  multiplier = bonusResult ? bonusResult->Fetch()[1].Get<float>() : 1.0f;

            handler->PSendSysMessage("[{}] Highest: {} | Chars at highest: {} | Bonus: {:.2f}x",
                factionName, highestLevel, cappedChars, multiplier);
        }

        return true;
    }
};

void AddSC_dynxp_commandscript()
{
    new dynxp_commandscript();
}
