/*
 * UnlockRacesClasses - AzerothCore fun module
 *
 * Features:
 *  - Unlocks all race/class combinations (including Death Knight for all races)
 *  - In-game race/class change via NPC master (gossip menu)
 *  - Custom specializations: Blademaster, Necromancer
 *  - Level requirement and item cost for changes
 *  - Server/DB/Client compatibility: menu contents filtered by server expansion,
 *    specializations persisted in the characters DB
 */

#ifndef _UNLOCK_RACES_CLASSES_H
#define _UNLOCK_RACES_CLASSES_H

#include "Player.h"
#include "ScriptMgr.h"
#include <chrono>
#include <unordered_map>
#include <vector>

// Custom specializations
enum UnlockSpec
{
    SPEC_NONE         = 0,
    SPEC_BLADEMASTER  = 1,
    SPEC_NECROMANCER  = 2
};

// Custom classes for the 3.3.5a client (ids 12/13 are unused by the vanilla client;
// they become visible only with the ChrClasses.dbc patch applied)
enum UrcCustomClass
{
    URC_CLASS_BLADEMASTER = 12,
    URC_CLASS_NECROMANCER = 13
};

struct UnlockChangeCost
{
    uint32 ItemEntry = 0; // 0 = free
    uint32 ItemCount = 0;
};

struct UnlockSpecData
{
    std::vector<uint32> Spells; // permanently learned
    std::vector<uint32> Auras;  // re-applied on login
};

struct UnlockStartLocation
{
    uint32 MapId = 0;
    float  X = 0.0f;
    float  Y = 0.0f;
    float  Z = 0.0f;
    float  O = 0.0f;
};

struct UnlockStartItem
{
    uint8  Race = 0;   // 0 = any race
    uint8  Class = 0;  // 0 = any class
    uint32 ItemEntry = 0;
    uint32 ItemCount = 1;
};

class UnlockRacesClasses
{
public:
    static UnlockRacesClasses* instance();

    // Loads config values (NPC entry, enabled flags, costs, spec spells)
    void LoadConfig();

    // Returns true if the given race/class combination is allowed by the module.
    // Checks client compatibility (expansion limits) and visual-only limitations.
    static bool IsCombinationAllowed(uint8 race, uint8 cls);

    // True if the client actually knows this class id (e.g. 3.3.5a client has no Monk/DH)
    static bool IsClassSupportedByClient(uint8 cls);

    // True if the client actually knows this race id for the current server expansion
    static bool IsRaceSupportedByClient(uint8 race);

    // Applies a race change to the player
    static bool ApplyRaceChange(Player* player, uint8 newRace);

    // Applies a class change to the player
    static bool ApplyClassChange(Player* player, uint8 newClass);

    // Applies a custom specialization (learn spells + cast auras, persisted in characters DB)
    static bool ApplySpecialization(Player* player, uint8 specId);

    // Helpers for level/cost checks (return false and message the player on failure)
    static bool MeetsLevelRequirement(Player* player);
    static bool CheckAndPayCost(Player* player, UnlockChangeCost const& cost, char const* actionName);

    // True when the player is in a state safe for a change (not in combat, alive, not on taxi)
    static bool IsPlayerChangeReady(Player* player);

    // Character DB persistence for specializations
    static void SavePlayerSpec(Player* player, uint8 specId);
    static uint8 LoadPlayerSpec(Player* player);

    // Character DB persistence for start items (one-time grant per character)
    static bool WasGivenStartItems(Player* player);
    static void MarkStartItemsGiven(Player* player);

    // Start locations: teleport after race change (custom start zones for new races)
    static void LoadStartLocations();
    static UnlockStartLocation const* GetStartLocation(uint8 race);

    // Start items: given after race/class change (race/class = 0 means "any")
    static void LoadStartItems();
    static void GiveStartItems(Player* player);

    // Change cooldown (seconds; persisted via player's in-memory cooldown map)
    static bool IsOnCooldown(Player* player);
    static void StartCooldown(Player* player);
    static void ClearCooldown(Player* player);
    static uint32 GetCooldownSeconds() { return _cooldownSeconds; }

    uint32 GetNpcEntry() const { return _npcEntry; }
    bool   IsEnabled() const { return _enabled; }
    uint32 GetMinLevel() const { return _minLevel; }

    // True when Cata+/MoP races (Goblin, Worgen, Pandaren) are enabled.
    // Requires matching client-side patches (see client-patch/README.md).
    bool   IsNewRacesEnabled() const { return _newRacesEnabled; }

    // True when custom classes (Blademaster 12, Necromancer 13) are enabled.
    // Requires matching client-side patches (see client-patch/README.md).
    bool   IsNewClassesEnabled() const { return _newClassesEnabled; }

    UnlockChangeCost const& GetRaceChangeCost() const { return _raceChangeCost; }
    UnlockChangeCost const& GetClassChangeCost() const { return _classChangeCost; }
    UnlockChangeCost const& GetSpecChangeCost() const { return _specChangeCost; }

    UnlockSpecData const* GetSpecData(uint8 specId) const;

private:
    uint32 _npcEntry = 0;
    bool   _enabled = true;
    uint32 _minLevel = 0;
    bool   _newRacesEnabled = false;
    bool   _newClassesEnabled = false;

    UnlockChangeCost _raceChangeCost;
    UnlockChangeCost _classChangeCost;
    UnlockChangeCost _specChangeCost;

    UnlockSpecData _specs[3]; // indexed by UnlockSpec

    std::unordered_map<uint8, UnlockStartLocation> _startLocations;
    std::vector<UnlockStartItem> _startItems;
    bool _teleportOnRaceChange = true;
    bool _giveStartItems = true;
    uint32 _cooldownSeconds = 0;

    // Per-player cooldown timestamps (not persisted; resets on server restart)
    std::unordered_map<uint32, std::chrono::steady_clock::time_point> _lastChange;
};

#define sUnlockRacesClasses UnlockRacesClasses::instance()

#endif // _UNLOCK_RACES_CLASSES_H
