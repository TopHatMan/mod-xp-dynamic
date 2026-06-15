-- Account linking
CREATE TABLE IF NOT EXISTS account_link_groups (
    account_id INT UNSIGNED NOT NULL PRIMARY KEY,
    group_id INT UNSIGNED NOT NULL,
    INDEX (group_id)
) ENGINE=InnoDB;

-- Faction-aware bonus (replaces account_xp_bonus logic)
-- faction: 0 = Alliance, 1 = Horde
ALTER TABLE account_xp_bonus ADD COLUMN faction TINYINT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE account_xp_bonus DROP PRIMARY KEY;
ALTER TABLE account_xp_bonus ADD PRIMARY KEY (account_id, faction);

ALTER TABLE account_highest_level ADD COLUMN faction TINYINT UNSIGNED NOT NULL DEFAULT 0;
ALTER TABLE account_highest_level DROP PRIMARY KEY;
ALTER TABLE account_highest_level ADD PRIMARY KEY (account_id, faction);