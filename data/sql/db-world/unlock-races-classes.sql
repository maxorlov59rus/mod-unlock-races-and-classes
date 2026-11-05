-- UnlockRacesClasses: race/class/spec change master NPC
-- One NPC template + spawns in all capital cities (Alliance and Horde).
-- Adjust models/coordinates to your realm if needed.

SET @NPC_ENTRY := 95000;

DELETE FROM `creature_template` WHERE `entry` = @NPC_ENTRY;
INSERT INTO `creature_template`
(`entry`, `modelid1`, `name`, `subname`, `iconname`, `gossip_menu_id`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `scale`, `rank`, `damagemultiplier`, `unit_class`, `unit_flags`, `type`, `AIName`, `ScriptName`)
VALUES
(@NPC_ENTRY, 28224, 'Мастер Перевоплощений', 'Мастер рас и классов', 'Taxi', 0, 80, 80, 35, 1, 1.2, 0, 1, 2, 2, 7, '', 'UnlockRacesClasses_NPC');

DELETE FROM `creature` WHERE `id1` = @NPC_ENTRY;

-- Alliance capitals
INSERT INTO `creature` (`id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`)
VALUES
-- Stormwind (near the bank)
(@NPC_ENTRY, 0, 12, 12, 1, 1, 0, -8833.86, 626.35, 94.30, 3.98, 300, 0, 0),
-- Ironforge (Great Forge)
(@NPC_ENTRY, 0, 1537, 1537, 1, 1, 0, -4795.65, -1127.80, 501.66, 0.00, 300, 0, 0),
-- Darnassus (Temple of the Moon)
(@NPC_ENTRY, 1, 1657, 1657, 1, 1, 0, 9619.42, 7119.60, 1218.13, 0.94, 300, 0, 0),
-- The Exodar (Vault of Lights)
(@NPC_ENTRY, 530, 3557, 3557, 1, 1, 0, -4054.32, -11353.20, -133.00, 0.42, 300, 0, 0);

-- Horde capitals
INSERT INTO `creature` (`id1`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`)
VALUES
-- Orgrimmar (Valley of Wisdom)
(@NPC_ENTRY, 1, 1637, 1637, 1, 1, 0, 1637.50, -4439.80, 15.42, 1.55, 300, 0, 0),
-- Undercity (The Trade Quarter)
(@NPC_ENTRY, 0, 1497, 1497, 1, 1, 0, 1622.80, 239.90, -43.10, 4.70, 300, 0, 0),
-- Thunder Bluff (Elder Rise)
(@NPC_ENTRY, 1, 1638, 1638, 1, 1, 0, -1268.30, 129.30, 131.60, 2.10, 300, 0, 0),
-- Silvermoon City (The Walk of Elders)
(@NPC_ENTRY, 530, 3487, 3487, 1, 1, 0, 9674.10, -7212.60, 12.90, 2.80, 300, 0, 0);
