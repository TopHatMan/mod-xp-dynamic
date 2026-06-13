-- mod-xp-dynamic: Required character database tables
-- Run against your AzerothCore characters database before starting the server.

-- Tracks the highest level reached by any character on each account.
-- Used by the alt progression bonus to gate the XP multiplier.
CREATE TABLE IF NOT EXISTS `account_highest_level` (
    `account_id`    INT UNSIGNED    NOT NULL,
    `highest_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic: highest level per account';

-- Stores the calculated alt XP bonus multiplier per account.
-- Recalculated whenever a character on the account reaches the server cap.
CREATE TABLE IF NOT EXISTS `account_xp_bonus` (
    `account_id`   INT UNSIGNED NOT NULL,
    `capped_chars` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `multiplier`   FLOAT NOT NULL DEFAULT 1.0,
    `last_updated` TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='mod-xp-dynamic: alt XP bonus multiplier per account';