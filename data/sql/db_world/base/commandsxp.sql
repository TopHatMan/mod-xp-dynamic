INSERT IGNORE INTO acore_world.command (name, security, help) VALUES
('dynxp account_link',   3, 'Syntax: .dynxp account_link <id1,id2,...> — Links accounts together for shared alt XP bonus. GM only.'),
('dynxp account_unlink', 3, 'Syntax: .dynxp account_unlink <id> — Removes an account from its link group. GM only.'),
('dynxp account_info',   0, 'Syntax: .dynxp account_info <id> — Shows link group and bonus info for an account.');