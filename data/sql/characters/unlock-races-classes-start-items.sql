-- UnlockRacesClasses: characters DB schema for one-time start items grant
-- Applied to the CHARACTERS database (not world).
--
-- Prevents farming start items by repeatedly changing race/class:
-- the module records here every character that already received its
-- start items via mod_urc_start_items.

CREATE TABLE IF NOT EXISTS `mod_unlock_races_classes_start_items`
(
    `guid` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;
