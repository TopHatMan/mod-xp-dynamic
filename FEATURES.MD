# mod-xp-dynamic — Feature Writeup

**Author:** TopHatMan (Nick)  
**AI Collaborator:** Claude (Anthropic)  
**Repository:** https://github.com/TopHatMan/mod-xp-dynamic  
**Built for:** AzerothCore WotLK 3.3.5a  

---

## Overview

`mod-xp-dynamic` is a comprehensive progression enhancement module for AzerothCore private servers. What started as a simple XP rate multiplier was extended over multiple development sessions into a full-featured progression system with dynamic rates, skill rewards, alt progression bonuses, cross-account linking, faction-gated bonuses, and ARAC class compatibility fixes.

Every feature was designed with a specific philosophy: **the world should feel dangerous early, rewards should feel meaningful, and long-term play should be rewarded.**

---

## Features

### 1. Dynamic XP Rate Brackets

XP rates are configurable per level range (1-4, 5-9, 10-14... all the way to 75-79), applied as a base multiplier on top of all XP sources. This creates a "bonsai curve" — early levels feel close to vanilla, later levels scale up to compensate for the increased content density.

**Source multipliers** stack on top of bracket rates:
- Kill XP: separate world vs dungeon rates
- Quest XP: separate world vs dungeon rates  
- Exploration XP: flat multiplier
- Bot solo rate: reduced XP for playerbots not grouped with real players

**Real-time XP feedback** — every XP gain shows the player a breakdown:
```
Ashbringer XP: 450 → 1012 (2.25x)
```

---

### 2. Dynamic Reputation Rate Brackets

Same bracket system as XP, applied to all reputation gains. Separate config toggle to enable/disable the alt bonus on rep gains (recommended off to prevent instant exalted on low-level city quests — learned the hard way).

**Real-time rep feedback:**
```
Ashbringer REP: 150 → 225 (1.50x)
```

---

### 3. Dynamic Gold Rate Brackets

Creature loot gold is scaled per level bracket. Designed with a "bonsai growth curve" — gold feels scarce at low levels and meaningful at high levels without making the economy trivial.

**Real-time gold feedback:**
```
Ashbringer GOLD: 45c → 104c (2.30x)
```

---

### 4. Skill XP Bonus Roll System

Every time a player gains a skill point (gathering, crafting, or combat skill), there is a chance to receive a bonus XP roll:

| Roll | Chance | Announcement |
|------|--------|--------------|
| +1 XP | 35% | Silent |
| +2 XP | 25% | Lucky! (green) |
| +3 XP | 20% | Nice roll! (blue) |
| +4 XP | 13% | Hot streak! (orange) |
| +5 XP | 7% | JACKPOT! (gold, announced to party) |

**Skill overflow** — on lucky rolls (2+), gathering and crafting skills can push past their skill cap by up to 5 points. A miner at 299/300 who rolls a 5 ends up at 304/300. Combat skills (weapons, defense) do not overflow.

Bots solo-leveling skills do not trigger the system unless grouped with a real player.

---

### 5. Alt Progression Bonus (Road to Cap)

The flagship feature. Rewards long-term play by giving alt characters bonus XP based on how many characters the account has at its personal highest level.

**How it works:**
- The system tracks the highest level character on each account
- When you level an alt below that highest level, you receive a bonus multiplier
- The bonus magnitude is based on how many characters are AT the highest level
- The gate is your personal highest level — not the server cap

**Example** with `PerMaxedChar = 0.25` and `MaxMultiplier = 5.0`:
- 1 character at level 20 (your highest) → alts at 1-19 get **1.25x** bonus
- 4 characters at level 20 → alts get **2.0x** bonus
- 16 characters at level 20 → alts get **5.0x** bonus (capped)

**Self-healing** — on every login the system scans all characters on the account to rebuild the table from scratch. Handles wiped databases, new installs, and playerbots that leveled while the real player was offline.

**Timing fix** — when a character levels up, their new level isn't saved to the database yet when the hook fires. The system excludes the current player from the DB query and adds them manually to avoid counting 0 capped characters.

**Bot awareness** — accounts with many level 1 bot characters don't skew the count. The system ignores accounts where the highest level is 1 or less.

---

### 6. Faction-Gated Alt Bonus

The alt bonus is **faction-specific**. Alliance characters only benefit from other Alliance characters on the account. Horde characters only benefit from other Horde characters.

This means to get the full alt bonus on both factions, you have to level both sides independently — creating meaningful long-term progression goals and encouraging players to experience both faction storylines.

- **Alliance races:** Human, Dwarf, Night Elf, Gnome, Draenei
- **Horde races:** Orc, Undead, Tauren, Troll, Blood Elf

---

### 7. Cross-Account Linking

Multiple accounts can be linked together into a bonus group. Linked accounts share their alt progression bonus — characters at the highest level across ALL linked accounts contribute to the multiplier.

This enables friend groups to share progression rewards across accounts while maintaining the faction gate.

**GM commands:**
```
.dynxp account_link 407,46469,12345    — Link accounts into a group
.dynxp account_unlink 407              — Remove account from group
.dynxp account_info 407               — View group and bonus info (available to all players)
```

---

### 8. ARAC Class Compatibility Fixes

Ashbringer runs with ARAC (Any Race Any Class) enabled via mod-playerbots. Some cross-faction class combinations cannot complete required trainer or quest chains because the NPC is on the wrong faction. This module automatically grants missing spells at the correct levels:

**Horde Paladin — Redemption:**
- Granted at level 12 to match Alliance Paladin trainer timing
- Spell ID 7328

**Alliance Shaman — Totem Unlocks:**
- Level 4: Earth Totem (spell 8073)
- Level 10: Fire Totem (spell 2075)  
- Level 20: Water Totem (spell 5396)
- Air Totem requires no spell unlock — the totem item is sufficient
- Totem items for all four elements granted at character creation via `playercreateinfo_item`

Both fixes are individually toggleable via config:
```ini
Dynamic.ClassFix.HordePaladin.Enable   = 1
Dynamic.ClassFix.AllianceShaman.Enable = 1
```

---

## Technical Highlights

### Database Tables

```sql
-- Tracks highest level per account per faction
account_highest_level (account_id, faction, highest_level)

-- Cached alt bonus multiplier per account per faction  
account_xp_bonus (account_id, faction, capped_chars, multiplier, last_updated)

-- Cross-account linking groups
account_link_groups (account_id, group_id)
```

### Config File

Fully documented `mod-xp-dynamic.conf.dist` with inline comments explaining every setting. All bracket rates, source multipliers, bot rates, skill XP toggle, alt bonus settings, class fix toggles — everything configurable without recompiling.

### String Formatting

Uses `Acore::StringFormat` (fmtlib wrapper) with `{}` placeholders for all dynamic chat messages. No legacy `%s`/`%u` format strings — these were found to be unreliable in PSendSysMessage for certain value types.

### Bot Safety

Every hook checks for playerbot sessions and skips or reduces rates appropriately:
- Skill XP roll: skipped for bots unless grouped with real player
- Alt bonus: faction-filtered to prevent level 1 bot accounts from skewing multipliers
- XP/rep/gold feedback messages: skipped for bots entirely

---

## Development Notes

This module was developed across multiple marathon sessions debugging an AzerothCore WotLK private server called **Ashbringer** — a two-player server with Chromie-style era-gated progression, Naxx40, LLM-powered playerbot companions, item affixes, a nemesis system, and more.

The alt bonus system went through several complete redesigns before landing on the current faction-gated approach. The key insight was that the bonus should track the account's **personal highest level** rather than the server cap — making it rewarding at every phase of progression rather than only after capping.

The cross-account linking system and faction gate were added to support a specific server design goal: rewarding dedication to both factions independently, while allowing friend groups to pool their progression across accounts.

---

## Installation

See `README.md` for full installation instructions including SQL setup and conf file placement.

**Required SQL** (run against characters database):
```bash
source data/sql/db_characters/base/account_tables.sql
```

---

*Built with love, sleep deprivation, and an unreasonable number of server crashes.*