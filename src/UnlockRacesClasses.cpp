/*
 * UnlockRacesClasses - AzerothCore fun module
 *
 * - Unlocks all race/class combinations
 * - NPC gossip: change race / change class / choose custom specialization
 * - Level requirement and item cost for all changes
 * - Server/DB/Client compatibility: menus filtered by client-known races/classes,
 *   specializations persisted in the characters DB
 */

#include "UnlockRacesClasses.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "ItemTemplate.h"
#include "Language.h"
#include "Log.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "WorldSession.h"
#include <algorithm>
#include <unordered_map>

namespace
{
    // Gossip action ids used by the NPC master
    constexpr uint32 GOSSIP_ACTION_RACE_MENU   = 100;
    constexpr uint32 GOSSIP_ACTION_CLASS_MENU  = 200;
    constexpr uint32 GOSSIP_ACTION_SPEC_MENU   = 300;

    // Human-readable class names for the menu
    std::unordered_map<uint8, std::string> const ClassNames =
    {
        { CLASS_WARRIOR,       "Воин" },
        { CLASS_PALADIN,       "Паладин" },
        { CLASS_HUNTER,        "Охотник" },
        { CLASS_ROGUE,         "Разбойник" },
        { CLASS_PRIEST,        "Жрец" },
        { CLASS_DEATH_KNIGHT,  "Рыцарь смерти" },
        { CLASS_SHAMAN,        "Шаман" },
        { CLASS_MAGE,          "Маг" },
        { CLASS_WARLOCK,       "Чернокнижник" },
        { CLASS_DRUID,         "Друид" },
    };

    // Race names for the menu
    std::unordered_map<uint8, std::string> const RaceNames =
    {
        { RACE_HUMAN,         "Человек" },
        { RACE_ORC,           "Орк" },
        { RACE_DWARF,         "Дворф" },
        { RACE_NIGHTELF,      "Ночной эльф" },
        { RACE_UNDEAD_PLAYER, "Нежить" },
        { RACE_TAUREN,        "Таурен" },
        { RACE_GNOME,         "Гном" },
        { RACE_TROLL,         "Тролль" },
        { RACE_BLOODELF,      "Эльф крови" },
        { RACE_DRAENEI,       "Дреней" },
    };

    std::unordered_map<uint8, std::string> const SpecNames =
    {
        { SPEC_BLADEMASTER, "Клинок-мастер (Blademaster)" },
        { SPEC_NECROMANCER, "Некромант (Necromancer)" },
    };

    // Cata+/MoP races enabled via config + client patches (see client-patch/)
    std::unordered_map<uint8, std::string> const NewRaceNames =
    {
        { RACE_GOBLIN,        "Гоблин" },
        { RACE_WORGEN,        "Ворген" },
        { RACE_PANDAREN_ALLI, "Пандарен (Альянс)" },
        { RACE_PANDAREN_HORDE,"Пандарен (Орда)" },
    };

    // Custom classes enabled via config + client patches (see client-patch/)
    std::unordered_map<uint8, std::string> const NewClassNames =
    {
        { URC_CLASS_BLADEMASTER, "Клинок-мастер (Blademaster)" },
        { URC_CLASS_NECROMANCER, "Некромант (Necromancer)" },
    };

    bool IsAllianceRace(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN:
            case RACE_DWARF:
            case RACE_NIGHTELF:
            case RACE_GNOME:
            case RACE_DRAENEI:
            case RACE_WORGEN:         // Cata+: Alliance
            case RACE_PANDAREN_ALLI:  // MoP+: Alliance
                return true;
            default:
                return false;
        }
    }

    bool IsHordeRace(uint8 race)
    {
        switch (race)
        {
            case RACE_ORC:
            case RACE_UNDEAD_PLAYER:
            case RACE_TAUREN:
            case RACE_TROLL:
            case RACE_BLOODELF:
            case RACE_GOBLIN:        // Cata+: Horde
            case RACE_PANDAREN_HORDE: // MoP+: Horde
                return true;
            default:
                return false;
        }
    }

    void AddCostOption(Player* player, char const* label, UnlockChangeCost const& cost, uint32 action)
    {
        std::string text(label);
        if (cost.ItemEntry && cost.ItemCount)
            text += " (цена: " + std::to_string(cost.ItemCount) + "x [" +
                    std::to_string(cost.ItemEntry) + "])";
        else
            text += " (бесплатно)";

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, text, GOSSIP_SENDER_MAIN, action);
    }
}

void UnlockRacesClasses::LoadConfig()
{
    _enabled  = sConfigMgr->GetBoolDefault("UnlockRacesClasses.Enable", true);
    _npcEntry = sConfigMgr->GetIntDefault("UnlockRacesClasses.NpcEntry", 95000);
    _minLevel = sConfigMgr->GetIntDefault("UnlockRacesClasses.MinLevel", 10);
    _newRacesEnabled = sConfigMgr->GetBoolDefault("UnlockRacesClasses.EnableNewRaces", false);
    _newClassesEnabled = sConfigMgr->GetBoolDefault("UnlockRacesClasses.EnableNewClasses", false);

    _raceChangeCost.ItemEntry = sConfigMgr->GetIntDefault("UnlockRacesClasses.RaceChange.ItemEntry", 0);
    _raceChangeCost.ItemCount = sConfigMgr->GetIntDefault("UnlockRacesClasses.RaceChange.ItemCount", 0);
    _classChangeCost.ItemEntry = sConfigMgr->GetIntDefault("UnlockRacesClasses.ClassChange.ItemEntry", 0);
    _classChangeCost.ItemCount = sConfigMgr->GetIntDefault("UnlockRacesClasses.ClassChange.ItemCount", 0);
    _specChangeCost.ItemEntry = sConfigMgr->GetIntDefault("UnlockRacesClasses.SpecChange.ItemEntry", 0);
    _specChangeCost.ItemCount = sConfigMgr->GetIntDefault("UnlockRacesClasses.SpecChange.ItemCount", 0);

    _specs[SPEC_BLADEMASTER].Spells = sConfigMgr->GetIntArrayDefault("UnlockRacesClasses.Spec.Blademaster.Spells", {});
    _specs[SPEC_BLADEMASTER].Auras  = sConfigMgr->GetIntArrayDefault("UnlockRacesClasses.Spec.Blademaster.Auras", {});
    _specs[SPEC_NECROMANCER].Spells = sConfigMgr->GetIntArrayDefault("UnlockRacesClasses.Spec.Necromancer.Spells", {});
    _specs[SPEC_NECROMANCER].Auras  = sConfigMgr->GetIntArrayDefault("UnlockRacesClasses.Spec.Necromancer.Auras", {});

    _teleportOnRaceChange = sConfigMgr->GetBoolDefault("UnlockRacesClasses.TeleportOnRaceChange", true);
    _giveStartItems = sConfigMgr->GetBoolDefault("UnlockRacesClasses.GiveStartItems", true);
    _cooldownSeconds = sConfigMgr->GetIntDefault("UnlockRacesClasses.ChangeCooldown", 0);

    LOG_INFO("module", "UnlockRacesClasses: loaded (enabled: {}, npc: {}, minLevel: {}, newRaces: {}, newClasses: {}, cooldown: {}s)",
        _enabled, _npcEntry, _minLevel, _newRacesEnabled, _newClassesEnabled, _cooldownSeconds);
}

// -------- Start locations (world DB, admin-configurable) --------

void UnlockRacesClasses::LoadStartLocations()
{
    _startLocations.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT race, mapId, positionX, positionY, positionZ, orientation "
        "FROM mod_urc_start_locations");

    if (!result)
    {
        LOG_INFO("module", "UnlockRacesClasses: no start locations defined");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint8 race = fields[0].Get<uint8>();

        UnlockStartLocation loc;
        loc.MapId = fields[1].Get<uint32>();
        loc.X = fields[2].Get<float>();
        loc.Y = fields[3].Get<float>();
        loc.Z = fields[4].Get<float>();
        loc.O = fields[5].Get<float>();

        _startLocations[race] = loc;
        LOG_INFO("module", "UnlockRacesClasses: start location for race {} (map {})",
            static_cast<uint32>(race), loc.MapId);
    } while (result->NextRow());
}

UnlockStartLocation const* UnlockRacesClasses::GetStartLocation(uint8 race)
{
    auto it = _startLocations.find(race);
    if (it != _startLocations.end())
        return &it->second;
    return nullptr;
}

// -------- Start items (world DB, given after race/class change) --------

void UnlockRacesClasses::LoadStartItems()
{
    _startItems.clear();

    QueryResult result = WorldDatabase.Query(
        "SELECT race, class, itemEntry, itemCount "
        "FROM mod_urc_start_items");

    if (!result)
    {
        LOG_INFO("module", "UnlockRacesClasses: no start items defined");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        UnlockStartItem entry;
        entry.Race = fields[0].Get<uint8>();
        entry.Class = fields[1].Get<uint8>();
        entry.ItemEntry = fields[2].Get<uint32>();
        entry.ItemCount = fields[3].Get<uint32>();

        _startItems.push_back(entry);
    } while (result->NextRow());

    LOG_INFO("module", "UnlockRacesClasses: loaded {} start item entries", _startItems.size());
}

void UnlockRacesClasses::GiveStartItems(Player* player)
{
    if (!player)
        return;

    // One-time grant per character: prevents farming start items
    // by repeatedly switching race/class when there is no cost/cooldown.
    if (WasGivenStartItems(player))
        return;

    uint8 race = player->getRace(true);
    uint8 cls = player->getClass();
    uint32 given = 0;

    for (UnlockStartItem const& entry : _startItems)
    {
        // race=0 and class=0 mean "any"; otherwise exact match is required
        if ((entry.Race && entry.Race != race) || (entry.Class && entry.Class != cls))
            continue;

        player->AddItem(entry.ItemEntry, entry.ItemCount);
        ++given;
    }

    if (given)
    {
        LOG_INFO("module", "UnlockRacesClasses: gave {} start item types to {}", given, player->GetName());
        MarkStartItemsGiven(player);
    }
}

// -------- Change cooldown --------

bool UnlockRacesClasses::IsOnCooldown(Player* player)
{
    if (!player || _cooldownSeconds == 0)
        return false;

    auto it = _lastChange.find(player->GetGUID().GetCounter());
    if (it == _lastChange.end())
        return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second).count();

    if (elapsed < static_cast<int64_t>(_cooldownSeconds))
    {
        uint32 remaining = static_cast<uint32>(_cooldownSeconds) - static_cast<uint32>(elapsed);
        player->SendSystemMessage("Смена будет доступна через " + std::to_string(remaining) + " сек.");
        return true;
    }

    return false;
}

void UnlockRacesClasses::StartCooldown(Player* player)
{
    if (player && _cooldownSeconds > 0)
        _lastChange[player->GetGUID().GetCounter()] = std::chrono::steady_clock::now();
}

void UnlockRacesClasses::ClearCooldown(Player* player)
{
    if (player)
        _lastChange.erase(player->GetGUID().GetCounter());
}

bool UnlockRacesClasses::IsClassSupportedByClient(uint8 cls)
{
    // WotLK 3.3.5a client knows classes 1-9 and 11 natively.
    // Custom classes 12/13 work only with the ChrClasses.dbc patch + config flag.
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_HUNTER:
        case CLASS_ROGUE:
        case CLASS_PRIEST:
        case CLASS_DEATH_KNIGHT:
        case CLASS_SHAMAN:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_DRUID:
            return true;
        case URC_CLASS_BLADEMASTER:
        case URC_CLASS_NECROMANCER:
            return sUnlockRacesClasses->IsNewClassesEnabled();
        default:
            return false; // Monk (MoP+), Demon Hunter (Legion+) need separate patches
    }
}

bool UnlockRacesClasses::IsRaceSupportedByClient(uint8 race)
{
    // Classic races are always supported by the 3.3.5a client.
    // Cata+/MoP races (Goblin 9, Worgen 22, Pandaren 24/25/26) are supported
    // only when the server admin applied the client patches AND enabled the config flag.
    switch (race)
    {
        case RACE_GOBLIN:
        case RACE_WORGEN:
        case RACE_PANDAREN_ALLI:
        case RACE_PANDAREN_HORDE:
        case RACE_PANDAREN_NEUTRAL:
            return sUnlockRacesClasses->IsNewRacesEnabled();
        default:
            return true;
    }
}

bool UnlockRacesClasses::IsCombinationAllowed(uint8 race, uint8 cls)
{
    if (race == RACE_NONE || cls == CLASS_NONE)
        return false;

    // Client compatibility first: never offer data the client cannot render
    if (!IsClassSupportedByClient(cls) || !IsRaceSupportedByClient(race))
        return false;

    // Visual-only limitations that would still work but look broken
    if (cls == CLASS_DRUID && race != RACE_NIGHTELF && race != RACE_TAUREN)
        return false; // druid forms models exist only for nelf/tauren

    return true;
}

bool UnlockRacesClasses::MeetsLevelRequirement(Player* player)
{
    if (!player)
        return false;

    uint32 minLevel = sUnlockRacesClasses->GetMinLevel();
    if (minLevel && player->getLevel() < minLevel)
    {
        player->SendSystemMessage("Минимальный уровень для смены: " + std::to_string(minLevel) + ".");
        return false;
    }
    return true;
}

bool UnlockRacesClasses::IsPlayerChangeReady(Player* player)
{
    if (!player)
        return false;

    if (!player->IsAlive())
    {
        player->SendSystemMessage("Смена возможна только в живом состоянии.");
        return false;
    }

    if (player->IsInCombat())
    {
        player->SendSystemMessage("Нельзя менять расу/класс/специализацию в бою.");
        return false;
    }

    if (player->isInFlight())
    {
        player->SendSystemMessage("Дождитесь окончания полёта на такси.");
        return false;
    }

    return true;
}

bool UnlockRacesClasses::CheckAndPayCost(Player* player, UnlockChangeCost const& cost, char const* actionName)
{
    if (!cost.ItemEntry || !cost.ItemCount)
        return true; // free

    if (!player->HasItemCount(cost.ItemEntry, cost.ItemCount))
    {
        player->SendSystemMessage(std::string("Для ") + actionName + " требуется: " +
            std::to_string(cost.ItemCount) + "x [" + std::to_string(cost.ItemEntry) + "].");
        return false;
    }

    player->DestroyItemCount(cost.ItemEntry, cost.ItemCount, true);
    return true;
}

bool UnlockRacesClasses::ApplyRaceChange(Player* player, uint8 newRace)
{
    if (!player || !IsCombinationAllowed(newRace, player->getClass()))
        return false;

    player->SetByteValue(UNIT_FIELD_BYTES_0, 0, newRace);
    player->setFactionForRace(newRace);

    player->InitTalentForLevel();
    player->InitTaxiNodesForLevel();
    player->SaveToDB(false, false);

    // Teleport to the configured start location for this race (if any)
    if (sUnlockRacesClasses->_teleportOnRaceChange)
    {
        if (UnlockStartLocation const* loc = GetStartLocation(newRace))
        {
            player->TeleportTo(loc->MapId, loc->X, loc->Y, loc->Z, loc->O);
        }
    }

    // Hand out start items for the new race/class
    if (sUnlockRacesClasses->_giveStartItems)
        GiveStartItems(player);

    LOG_INFO("module", "UnlockRacesClasses: {} changed race to {}",
        player->GetName(), static_cast<uint32>(newRace));

    return true;
}

bool UnlockRacesClasses::ApplyClassChange(Player* player, uint8 newClass)
{
    if (!player || !IsCombinationAllowed(player->getRace(true), newClass))
        return false;

    player->SetByteValue(UNIT_FIELD_BYTES_0, 2, newClass);
    player->InitTalentForLevel();
    player->SaveToDB(false, false);

    // Hand out start items for the new race/class
    if (sUnlockRacesClasses->_giveStartItems)
        GiveStartItems(player);

    LOG_INFO("module", "UnlockRacesClasses: {} changed class to {}",
        player->GetName(), static_cast<uint32>(newClass));

    return true;
}

UnlockSpecData const* UnlockRacesClasses::GetSpecData(uint8 specId) const
{
    if (specId > SPEC_NONE && specId <= SPEC_NECROMANCER)
        return &_specs[specId];
    return nullptr;
}

bool UnlockRacesClasses::ApplySpecialization(Player* player, uint8 specId)
{
    UnlockSpecData const* data = GetSpecData(specId);
    if (!player || !data)
        return false;

    // Learn configured spells permanently
    for (uint32 spell : data->Spells)
    {
        if (sSpellMgr->GetSpellInfo(spell))
            player->learnSpell(spell, false);
    }

    // Cast configured auras (re-applied on login)
    for (uint32 aura : data->Auras)
    {
        if (sSpellMgr->GetSpellInfo(aura))
            player->CastSpell(player, aura, true);
    }

    // Persist so it survives relogs and is restored server-side
    SavePlayerSpec(player, specId);

    LOG_INFO("module", "UnlockRacesClasses: {} took specialization {}",
        player->GetName(), static_cast<uint32>(specId));

    return true;
}

// -------- Characters DB persistence for specializations --------

void UnlockRacesClasses::SavePlayerSpec(Player* player, uint8 specId)
{
    if (!player)
        return;

    // Single transaction: DELETE + INSERT must be atomic, otherwise with a
    // multi-connection async pool the DELETE may run after the INSERT and
    // the persisted specialization would be lost.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

    trans->Append("DELETE FROM mod_unlock_races_classes_specs WHERE guid = {}",
        player->GetGUID().GetCounter());
    trans->Append("INSERT INTO mod_unlock_races_classes_specs (guid, specId) VALUES ({}, {})",
        player->GetGUID().GetCounter(), specId);

    CharacterDatabase.CommitTransaction(trans);
}

// -------- Characters DB persistence for start items (one-time grant) --------

bool UnlockRacesClasses::WasGivenStartItems(Player* player)
{
    if (!player)
        return true; // fail-safe: do not grant

    QueryResult result = CharacterDatabase.Query(
        "SELECT 1 FROM mod_unlock_races_classes_start_items WHERE guid = {}",
        player->GetGUID().GetCounter());

    return result != nullptr;
}

void UnlockRacesClasses::MarkStartItemsGiven(Player* player)
{
    if (!player)
        return;

    CharacterDatabase.Execute(
        "REPLACE INTO mod_unlock_races_classes_start_items (guid) VALUES ({})",
        player->GetGUID().GetCounter());
}

uint8 UnlockRacesClasses::LoadPlayerSpec(Player* player)
{
    if (!player)
        return SPEC_NONE;

    // Loaded at login by PlayerScript::OnLoadFromDB (async query); kept simple:
    // synchronous read is safe here for module-level fun features.
    QueryResult result = CharacterDatabase.Query(
        "SELECT specId FROM mod_unlock_races_classes_specs WHERE guid = {}",
        player->GetGUID().GetCounter());

    if (!result)
        return SPEC_NONE;

    return result->Fetch()[0].Get<uint8>();
}

UnlockRacesClasses* UnlockRacesClasses::instance()
{
    static UnlockRacesClasses instance;
    return &instance;
}

// ---------------- Script hooks ----------------

class UnlockRacesClasses_WorldScript : public WorldScript
{
public:
    UnlockRacesClasses_WorldScript() : WorldScript("UnlockRacesClasses_WorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sUnlockRacesClasses->LoadConfig();
    }

    void OnStartup() override
    {
        UnlockRacesClasses::LoadStartLocations();
        UnlockRacesClasses::LoadStartItems();
    }
};

class UnlockRacesClasses_PlayerScript : public PlayerScript
{
public:
    UnlockRacesClasses_PlayerScript() : PlayerScript("UnlockRacesClasses_PlayerScript") { }

    // Re-apply specialization auras after login (persisted in characters DB)
    void OnLogin(Player* player) override
    {
        uint8 specId = sUnlockRacesClasses->LoadPlayerSpec(player);
        if (specId == SPEC_NONE)
            return;

        UnlockSpecData const* data = sUnlockRacesClasses->GetSpecData(specId);
        if (!data)
            return;

        for (uint32 aura : data->Auras)
        {
            if (sSpellMgr->GetSpellInfo(aura) && !player->HasAura(aura))
                player->CastSpell(player, aura, true);
        }
    }

    // Cleanup: drop the per-player cooldown timestamp so the map doesn't grow
    void OnLogout(Player* player) override
    {
        sUnlockRacesClasses->ClearCooldown(player);
    }
};

class UnlockRacesClasses_NPC : public CreatureScript
{
public:
    UnlockRacesClasses_NPC() : CreatureScript("UnlockRacesClasses_NPC") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sUnlockRacesClasses->IsEnabled() ||
            creature->GetEntry() != sUnlockRacesClasses->GetNpcEntry())
        {
            return false;
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Сменить расу",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_RACE_MENU);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Сменить класс",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_CLASS_MENU);
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_2, "Выбрать специализацию",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_SPEC_MENU);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!sUnlockRacesClasses->IsEnabled() ||
            creature->GetEntry() != sUnlockRacesClasses->GetNpcEntry())
        {
            return false;
        }

        ClearGossipMenuFor(player);

        uint8 currentRace = player->getRace(true);
        uint8 currentClass = player->getClass();

        if (action == GOSSIP_ACTION_RACE_MENU)
        {
            // Classic races always, Cata+/MoP races only when enabled+patched
            for (auto const& [race, name] : RaceNames)
            {
                if (race == currentRace || !UnlockRacesClasses::IsRaceSupportedByClient(race))
                    continue;

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, name,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_RACE_MENU + race);
            }

            if (sUnlockRacesClasses->IsNewRacesEnabled())
            {
                for (auto const& [race, name] : NewRaceNames)
                {
                    if (race == currentRace)
                        continue;

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, name,
                        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_RACE_MENU + race);
                }
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
            return true;
        }

        if (action == GOSSIP_ACTION_CLASS_MENU)
        {
            // Native classes always, custom classes only when enabled+patched
            for (auto const& [cls, name] : ClassNames)
            {
                if (cls == currentClass || !UnlockRacesClasses::IsClassSupportedByClient(cls))
                    continue;

                AddGossipItemFor(player, GOSSIP_ICON_CHAT, name,
                    GOSSIP_SENDER_MAIN, GOSSIP_ACTION_CLASS_MENU + cls);
            }

            if (sUnlockRacesClasses->IsNewClassesEnabled())
            {
                for (auto const& [cls, name] : NewClassNames)
                {
                    if (cls == currentClass)
                        continue;

                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, name,
                        GOSSIP_SENDER_MAIN, GOSSIP_ACTION_CLASS_MENU + cls);
                }
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
            return true;
        }

        if (action == GOSSIP_ACTION_SPEC_MENU)
        {
            for (auto const& [spec, name] : SpecNames)
                AddCostOption(player, name.c_str(), sUnlockRacesClasses->GetSpecChangeCost(),
                    GOSSIP_ACTION_SPEC_MENU + spec);

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature);
            return true;
        }

        // --- Race change ---
        if (action > GOSSIP_ACTION_RACE_MENU && action < GOSSIP_ACTION_CLASS_MENU)
        {
            uint8 newRace = static_cast<uint8>(action - GOSSIP_ACTION_RACE_MENU);
            CloseGossipMenuFor(player);

            // Validate the combination before charging the cost (no refunds)
            if (!UnlockRacesClasses::IsCombinationAllowed(newRace, currentClass))
            {
                player->SendSystemMessage("Эта комбинация недоступна.");
                return true;
            }

            if (!UnlockRacesClasses::MeetsLevelRequirement(player))
                return true;

            if (!UnlockRacesClasses::IsPlayerChangeReady(player))
                return true;

            if (sUnlockRacesClasses->IsOnCooldown(player))
                return true;

            if (!UnlockRacesClasses::CheckAndPayCost(player, sUnlockRacesClasses->GetRaceChangeCost(), "смены расы"))
                return true;

            if (sUnlockRacesClasses->ApplyRaceChange(player, newRace))
            {
                sUnlockRacesClasses->StartCooldown(player);
                player->SendSystemMessage("Ваша раса изменена! Перезайдите для полного обновления модели.");
            }
            else
                player->SendSystemMessage("Эта комбинация недоступна.");

            return true;
        }

        // --- Class change ---
        if (action > GOSSIP_ACTION_CLASS_MENU && action < GOSSIP_ACTION_SPEC_MENU)
        {
            uint8 newClass = static_cast<uint8>(action - GOSSIP_ACTION_CLASS_MENU);
            CloseGossipMenuFor(player);

            // Validate the combination before charging the cost (no refunds)
            if (!UnlockRacesClasses::IsCombinationAllowed(currentRace, newClass))
            {
                player->SendSystemMessage("Эта комбинация недоступна.");
                return true;
            }

            if (!UnlockRacesClasses::MeetsLevelRequirement(player))
                return true;

            if (!UnlockRacesClasses::IsPlayerChangeReady(player))
                return true;

            if (sUnlockRacesClasses->IsOnCooldown(player))
                return true;

            if (!UnlockRacesClasses::CheckAndPayCost(player, sUnlockRacesClasses->GetClassChangeCost(), "смены класса"))
                return true;

            if (sUnlockRacesClasses->ApplyClassChange(player, newClass))
            {
                sUnlockRacesClasses->StartCooldown(player);
                player->SendSystemMessage("Ваш класс изменён! Спеллы и таланты будут перераспределены при следующем входе.");
            }
            else
                player->SendSystemMessage("Эта комбинация недоступна.");

            return true;
        }

        // --- Specialization ---
        if (action > GOSSIP_ACTION_SPEC_MENU)
        {
            uint8 specId = static_cast<uint8>(action - GOSSIP_ACTION_SPEC_MENU);
            CloseGossipMenuFor(player);

            if (!UnlockRacesClasses::MeetsLevelRequirement(player))
                return true;

            if (!UnlockRacesClasses::IsPlayerChangeReady(player))
                return true;

            if (sUnlockRacesClasses->IsOnCooldown(player))
                return true;

            if (!UnlockRacesClasses::CheckAndPayCost(player, sUnlockRacesClasses->GetSpecChangeCost(), "выбора специализации"))
                return true;

            if (sUnlockRacesClasses->ApplySpecialization(player, specId))
            {
                sUnlockRacesClasses->StartCooldown(player);
                player->SendSystemMessage("Специализация получена! Ауры будут восстанавливаться при входе в игру.");
            }
            else
                player->SendSystemMessage("Неизвестная специализация.");

            return true;
        }

        return false;
    }
};

// ---------------- Admin commands: .urc race <id> / .urc class <id> ----------------

class UnlockRacesClasses_CommandScript : public CommandScript
{
public:
    UnlockRacesClasses_CommandScript() : CommandScript("UnlockRacesClasses_CommandScript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> urcCommandTable =
        {
            { "race",  SEC_ADMINISTRATOR, false, &HandleRaceCommand, "" },
            { "class", SEC_ADMINISTRATOR, false, &HandleClassCommand, "" },
        };

        static std::vector<ChatCommand> urcuiCommandTable =
        {
            { "status", SEC_PLAYER, false, &HandleUiStatusCommand, "" },
            { "race",   SEC_PLAYER, false, &HandleUiRaceCommand, "" },
            { "class",  SEC_PLAYER, false, &HandleUiClassCommand, "" },
            { "spec",   SEC_PLAYER, false, &HandleUiSpecCommand, "" },
        };

        static std::vector<ChatCommand> commandTable =
        {
            { "urc",  SEC_ADMINISTRATOR, false, nullptr, "", urcCommandTable },
            { "urcui", SEC_PLAYER,      false, nullptr, "", urcuiCommandTable },
        };

        return commandTable;
    }

    static bool HandleRaceCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("Использование: .urc race <raceId> (цель: выбранный игрок или вы сами)");
            return false;
        }

        Player* target = handler->getSelectedPlayer();
        if (!target)
            target = handler->GetSession()->GetPlayer();

        uint32 raceId = static_cast<uint32>(atoi(args));
        if (raceId > RACE_PANDAREN_NEUTRAL)
        {
            handler->SendSysMessage("Некорректный ID расы.");
            return false;
        }

        if (!sUnlockRacesClasses->ApplyRaceChange(target, static_cast<uint8>(raceId)))
        {
            handler->SendSysMessage("Не удалось изменить расу (комбинация недоступна).");
            return false;
        }

        handler->PSendSysMessage("Раса {} изменена на {}.", target->GetName(), raceId);
        return true;
    }

    static bool HandleClassCommand(ChatHandler* handler, char const* args)
    {
        if (!*args)
        {
            handler->SendSysMessage("Использование: .urc class <classId> (цель: выбранный игрок или вы сами)");
            return false;
        }

        Player* target = handler->getSelectedPlayer();
        if (!target)
            target = handler->GetSession()->GetPlayer();

        uint32 classId = static_cast<uint32>(atoi(args));
        if (classId == 0 || classId > URC_CLASS_NECROMANCER)
        {
            handler->SendSysMessage("Некорректный ID класса.");
            return false;
        }

        if (!sUnlockRacesClasses->ApplyClassChange(target, static_cast<uint8>(classId)))
        {
            handler->SendSysMessage("Не удалось изменить класс (комбинация недоступна).");
            return false;
        }

        handler->PSendSysMessage("Класс {} изменён на {}.", target->GetName(), classId);
        return true;
    }

    // ---- Player-facing commands used by the UI addon ----
    // .urcui status          -> prints current race/class/spec
    // .urcui race <id>       -> NPC-equivalent race change (checks/cost/cooldown apply)
    // .urcui class <id>      -> NPC-equivalent class change
    // .urcui spec <id>       -> NPC-equivalent specialization choice

    static bool HandleUiStatusCommand(ChatHandler* handler, char const* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint8 spec = sUnlockRacesClasses->LoadPlayerSpec(player);
        handler->PSendSysMessage("URC_STATUS race={} class={} spec={}",
            static_cast<uint32>(player->getRace(true)),
            static_cast<uint32>(player->getClass()),
            static_cast<uint32>(spec));
        return true;
    }

    static bool HandleUiRaceCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !*args)
            return false;

        uint32 raceId = static_cast<uint32>(atoi(args));

        if (!UnlockRacesClasses::MeetsLevelRequirement(player) ||
            sUnlockRacesClasses->IsOnCooldown(player) ||
            !UnlockRacesClasses::CheckAndPayCost(player, sUnlockRacesClasses->GetRaceChangeCost(), "смены расы"))
            return true;

        if (sUnlockRacesClasses->ApplyRaceChange(player, static_cast<uint8>(raceId)))
        {
            sUnlockRacesClasses->StartCooldown(player);
            handler->PSendSysMessage("URC_OK race={}", raceId);
        }
        else
            handler->SendSysMessage("URC_FAIL комбинация недоступна");

        return true;
    }

    static bool HandleUiClassCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !*args)
            return false;

        uint32 classId = static_cast<uint32>(atoi(args));

        if (!UnlockRacesClasses::MeetsLevelRequirement(player) ||
            sUnlockRacesClasses->IsOnCooldown(player) ||
            !UnlockRacesClasses::CheckAndPayCost(player, sUnlockRacesClasses->GetClassChangeCost(), "смены класса"))
            return true;

        if (sUnlockRacesClasses->ApplyClassChange(player, static_cast<uint8>(classId)))
        {
            sUnlockRacesClasses->StartCooldown(player);
            handler->PSendSysMessage("URC_OK class={}", classId);
        }
        else
            handler->SendSysMessage("URC_FAIL комбинация недоступна");

        return true;
    }

    static bool HandleUiSpecCommand(ChatHandler* handler, char const* args)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !*args)
            return false;

        uint32 specId = static_cast<uint32>(atoi(args));

        if (!UnlockRacesClasses::MeetsLevelRequirement(player) ||
            sUnlockRacesClasses->IsOnCooldown(player) ||
            !UnlockRacesClasses::CheckAndPayCost(player, sUnlockRacesClasses->GetSpecChangeCost(), "выбора специализации"))
            return true;

        if (sUnlockRacesClasses->ApplySpecialization(player, static_cast<uint8>(specId)))
        {
            sUnlockRacesClasses->StartCooldown(player);
            handler->PSendSysMessage("URC_OK spec={}", specId);
        }
        else
            handler->SendSysMessage("URC_FAIL неизвестная специализация");

        return true;
    }
};

void AddSC_UnlockRacesClasses()
{
    new UnlockRacesClasses_WorldScript();
    new UnlockRacesClasses_PlayerScript();
    new UnlockRacesClasses_NPC();
    new UnlockRacesClasses_CommandScript();
}
