#include "Chat.h"
#include "ChatCommand.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "StringFormat.h"
#include <sstream>
#include <vector>

class dynxp_commandscript : public CommandScript
{
public:
    dynxp_commandscript() : CommandScript("dynxp_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable dynxpCommandTable =
        {
            { "account_link",  HandleAccountLink,  SEC_CONSOLE, Console::Yes },
            { "account_unlink", HandleAccountUnlink, SEC_CONSOLE, Console::Yes },
            { "account_info",  HandleAccountInfo,  SEC_CONSOLE, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "dynxp", dynxpCommandTable },
        };

        return commandTable;
    }

    // ── dynxp account_link x,x,x ──────────────────────────────────────
    // Links multiple accounts together under one bonus group.
    // If any account is already in a group, all accounts join that group.
    // Usage: dynxp account_link 407,46469,12345

    static bool HandleAccountLink(ChatHandler* handler, std::string const& args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("Usage: dynxp account_link <id1,id2,id3,...>");
            return false;
        }

        // Parse comma-separated account IDs
        std::vector<uint32> accountIds;
        std::stringstream ss(args);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            if (token.empty()) continue;

            try
            {
                uint32 id = static_cast<uint32>(std::stoul(token));
                accountIds.push_back(id);
            }
            catch (...)
            {
                handler->PSendSysMessage("Invalid account ID: '{}'", token);
                return false;
            }
        }

        if (accountIds.size() < 2)
        {
            handler->SendSysMessage("Need at least 2 account IDs to link.");
            return false;
        }

        // Check if any account already has a group — use that group_id
        uint32 groupId = 0;
        for (uint32 id : accountIds)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT group_id FROM account_link_groups WHERE account_id = {}",
                id);
            if (result)
            {
                groupId = result->Fetch()[0].Get<uint32>();
                break;
            }
        }

        // No existing group — create new group_id from MAX + 1
        if (groupId == 0)
        {
            QueryResult result = CharacterDatabase.Query(
                "SELECT MAX(group_id) FROM account_link_groups");
            groupId = result ? (result->Fetch()[0].Get<uint32>() + 1) : 1;
        }

        // Insert or update all accounts into the group
        for (uint32 id : accountIds)
        {
            CharacterDatabase.Execute(
                "INSERT INTO account_link_groups (account_id, group_id) VALUES ({}, {}) "
                "ON DUPLICATE KEY UPDATE group_id = {}",
                id, groupId, groupId);
        }

        handler->PSendSysMessage("|cff00FF00Linked {} accounts under group {}:|r", accountIds.size(), groupId);
        for (uint32 id : accountIds)
            handler->PSendSysMessage("  → Account {}", id);

        return true;
    }

    // ── dynxp account_unlink x ────────────────────────────────────────
    // Removes a single account from its link group.
    // Usage: dynxp account_unlink 407

    static bool HandleAccountUnlink(ChatHandler* handler, std::string const& args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("Usage: dynxp account_unlink <account_id>");
            return false;
        }

        uint32 accountId = static_cast<uint32>(std::stoul(args));

        QueryResult result = CharacterDatabase.Query(
            "SELECT group_id FROM account_link_groups WHERE account_id = {}",
            accountId);

        if (!result)
        {
            handler->PSendSysMessage("Account {} is not in any link group.", accountId);
            return false;
        }

        uint32 groupId = result->Fetch()[0].Get<uint32>();

        CharacterDatabase.Execute(
            "DELETE FROM account_link_groups WHERE account_id = {}",
            accountId);

        handler->PSendSysMessage("|cffFFD700Account {} removed from group {}.|r", accountId, groupId);
        return true;
    }

    // ── dynxp account_info x ──────────────────────────────────────────
    // Shows link group and bonus info for an account.
    // Usage: dynxp account_info 407

    static bool HandleAccountInfo(ChatHandler* handler, std::string const& args)
    {
        if (args.empty())
        {
            handler->SendSysMessage("Usage: dynxp account_info <account_id>");
            return false;
        }

        uint32 accountId = static_cast<uint32>(std::stoul(args));

        // Group info
        QueryResult groupResult = CharacterDatabase.Query(
            "SELECT group_id FROM account_link_groups WHERE account_id = {}",
            accountId);

        if (groupResult)
        {
            uint32 groupId = groupResult->Fetch()[0].Get<uint32>();
            handler->PSendSysMessage("Account {} is in link group {}.", accountId, groupId);

            // Show all members of the group
            QueryResult members = CharacterDatabase.Query(
                "SELECT account_id FROM account_link_groups WHERE group_id = {}",
                groupId);
            if (members)
            {
                handler->SendSysMessage("Group members:");
                do
                {
                    handler->PSendSysMessage("  → Account {}", members->Fetch()[0].Get<uint32>());
                } while (members->NextRow());
            }
        }
        else
        {
            handler->PSendSysMessage("Account {} has no link group.", accountId);
        }

        // Bonus info per faction
        for (uint8 faction = 0; faction <= 1; ++faction)
        {
            const char* factionName = (faction == 0) ? "Alliance" : "Horde";

            QueryResult hlResult = CharacterDatabase.Query(
                "SELECT highest_level FROM account_highest_level "
                "WHERE account_id = {} AND faction = {}",
                accountId, faction);

            QueryResult bonusResult = CharacterDatabase.Query(
                "SELECT capped_chars, multiplier FROM account_xp_bonus "
                "WHERE account_id = {} AND faction = {}",
                accountId, faction);

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
