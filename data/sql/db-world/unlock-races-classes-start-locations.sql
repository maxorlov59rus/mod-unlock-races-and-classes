-- UnlockRacesClasses: start locations for races (applied to WORLD database).
-- The module teleports a player here after a race change (config-gated).
--
-- For classic races the native start zones are used by default.
-- For new races (Goblin/Worgen/Pandaren) there are no native 3.3.5a start
-- zones, so they start in their faction capitals; adjust coordinates freely.
--
-- Position templates (safe spots, adjust for your realm):
--   Stormwind  (map 0):  -8833.86  626.35  94.30  o 3.98
--   Orgrimmar  (map 1):   1637.50 -4439.80 15.42  o 1.55

DROP TABLE IF EXISTS `mod_urc_start_locations`;
CREATE TABLE `mod_urc_start_locations`
(
    `race`       TINYINT UNSIGNED NOT NULL,
    `mapId`      SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `positionX`  FLOAT NOT NULL DEFAULT 0,
    `positionY`  FLOAT NOT NULL DEFAULT 0,
    `positionZ`  FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0,
    PRIMARY KEY (`race`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;

-- New races: no native 3.3.5a start zones -> faction capitals
INSERT INTO `mod_urc_start_locations` (`race`, `mapId`, `positionX`, `positionY`, `positionZ`, `orientation`) VALUES
-- Goblin (9) -> Orgrimmar
(9,  1, 1637.50, -4439.80, 15.42, 1.55),
-- Worgen (22) -> Stormwind
(22, 0, -8833.86, 626.35, 94.30, 3.98),
-- Pandaren Alliance (24) -> Stormwind
(24, 0, -8833.86, 626.35, 94.30, 3.98),
-- Pandaren Horde (25) -> Orgrimmar
(25, 1, 1637.50, -4439.80, 15.42, 1.55);
