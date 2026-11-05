-- UnlockRacesClasses: characters DB schema for specialization persistence
-- Applied to the CHARACTERS database (not world).

CREATE TABLE IF NOT EXISTS `mod_unlock_races_classes_specs`
(
    `guid`   INT UNSIGNED NOT NULL,
    `specId` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4 COLLATE = utf8mb4_unicode_ci;
