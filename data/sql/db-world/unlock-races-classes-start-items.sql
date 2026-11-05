-- UnlockRacesClasses: start items per race/class (applied to WORLD database).
-- Given automatically after a race or class change (config-gated).
--
-- Semantics:
--   race = 0  -> item is given regardless of race
--   class = 0 -> item is given regardless of class
--   Both nonzero -> exact race+class match required.
--
-- Default sets below: basic weapons/gear placeholders for new races.
-- Replace itemEntry values with your realm's custom kit entries if you have them.

DROP TABLE IF EXISTS `mod_urc_start_items`;
CREATE TABLE `mod_urc_start_items`
(
    `race`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `class`     TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `itemEntry` INT UNSIGNED NOT NULL,
    `itemCount` INT UNSIGNED NOT NULL DEFAULT 1,
    KEY `idx_race_class` (`race`, `class`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;

-- ==== Universal (any race/class) ====
INSERT INTO `mod_urc_start_items` (`race`, `class`, `itemEntry`, `itemCount`) VALUES
(0, 0, 6948, 1),    -- Hearthstone
(0, 0, 159, 20),    -- Refreshing Spring Water (placeholder)
(9, 1, 25, 1),      -- Goblin warrior: Worn Shortsword (placeholder)
(22, 1, 25, 1),     -- Worgen warrior: Worn Shortsword (placeholder)
(24, 1, 25, 1),     -- Pandaren Alliance warrior: Worn Shortsword (placeholder)
(25, 1, 25, 1);     -- Pandaren Horde warrior: Worn Shortsword (placeholder)
