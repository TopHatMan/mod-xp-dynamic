# mod-xp-dynamic

An AzerothCore module that replaces flat XP/gold/reputation rates with a fully configurable system including per-level-bracket multipliers, source-based XP scaling, a skill gain bonus roll, and an alt progression bonus.

Built for WotLK 3.3.5a.

---

## Features

### Dynamic XP Rates
Per-level-bracket multipliers from 1–79, stacked with source multipliers for kills, quests, and exploration. Dungeon content rewards more XP than open world to encourage grouping.

### Dynamic Gold Rates
Per-level-bracket multiplier applied to creature loot gold. Scales with level to keep gold meaningful across the entire progression.

### Dynamic Reputation Rates
Per-level-bracket multiplier applied to all reputation gains. Recommended to keep these modest — see the warning about city faction rep in the conf file.

### Skill XP Bonus Roll
Every time a player earns a skill point (gathering, crafting, or combat skill), there is a chance to receive a small bonus XP roll:

| Roll | Chance | Message |
|------|--------|---------|
| +1 XP | 35% | Silent |
| +2 XP | 25% | Lucky! (green) |
| +3 XP | 20% | Nice roll! (blue) |
| +4 XP | 13% | Hot streak! (orange) |
| +5 XP | 7% | JACKPOT! (gold, announced to group) |

Gathering and crafting skills can also roll **past their skill cap by up to 5 points** on lucky rolls. Combat skills (weapons, defense) do not overflow.

### Alt Progression Bonus (Road to Cap)
Each time a character on your account reaches the server level cap, all your lower-level alts receive an XP bonus — rewarding long-term play and encouraging you to explore different classes.

**How it works:**
- The bonus applies only when you are **below the highest level character on your account**
- The multiplier is based on how many characters you have at the server cap
- Configurable per-character bonus and maximum cap

**Example** with `PerMaxedChar = 0.25` and `MaxMultiplier = 5.0`:
- 1 capped character → alts get **1.25x** XP
- 4 capped characters → alts get **2.0x** XP
- 16 capped characters → alts get **5.0x** XP (max)

### Class Fixes (ARAC Support)
When using mods that enable Any Race Any Class (e.g. mod-playerbots with ARAC), some classes cannot complete required trainer/quest chains because the NPC is on the wrong faction. This module automatically grants missing spells at the correct level:

- **Horde Paladin** — Redemption granted at level 12
- **Alliance Shaman** — Totem unlock spells granted at levels 4, 10, and 20

These fixes are configurable and can be disabled if you are not running ARAC.

---

## Installation

### 1. Clone into your AzerothCore modules folder
```bash
cd /path/to/azerothcore/modules
git clone https://github.com/YOUR_GITHUB/mod-xp-dynamic.git
```

### 2. Rebuild the server
```bash
cmake ../ -DCMAKE_INSTALL_PREFIX=/path/to/server
make -j$(nproc)
make install
```

### 3. Apply required SQL
Run against your **characters database**:
```bash
mysql -u root -p your_characters_db < data/sql/db_characters/base/account_tables.sql
```

### 4. Configure
Copy the conf file:
```bash
cp conf/mod-xp-dynamic.conf.dist /path/to/server/etc/modules/mod-xp-dynamic.conf
```
Edit `mod-xp-dynamic.conf` to your liking.

### 5. (Optional) Alliance Shaman totem items
If using ARAC, grant totem items to all races for the Shaman class at character creation. Run against your **world database**:

```sql
INSERT IGNORE INTO playercreateinfo_item (race, class, itemid, amount)
SELECT race, 7, item, 1 FROM (
    SELECT race FROM playercreateinfo GROUP BY race
) races CROSS JOIN (
    SELECT 5175 AS item UNION SELECT 5176 UNION SELECT 5177 UNION SELECT 5178
) items;
```

Items: Earth Totem (5175), Fire Totem (5176), Water Totem (5177), Air Totem (5178).

---

## Requirements

- AzerothCore WotLK 3.3.5a (recent master branch)
- MySQL 5.7+ or MariaDB 10.3+
- Optional: mod-playerbots (for bot rate and ARAC class fix features)

---

## Configuration Reference

See `conf/mod-xp-dynamic.conf.dist` — every option is fully documented inline.

---

## Credits

- Original script: [AshmaneCore Single Player Project](https://github.com/conan513)
- Reworked by: Micrah/Milestorme and Poszer
- Extended with Gold, Reputation, Alt Progression Bonus, Skill XP, and Class Fixes