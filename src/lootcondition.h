#ifndef LOOTCONDITION_H
#define LOOTCONDITION_H

#include "cubiomes/finders.h"
#include "cubiomes/loot.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <map>

struct LootRule
{
    int item = DP_LOOT_DIAMOND;
    int minCount = 1;
    int maxCount = -1; // negative means no upper bound
    int enchantment = -1; // any enchantment / not applicable
    int minLevel = 1;
    int maxLevel = DP_ENCH_MAX_LEVEL;
};

struct LootRuleSet
{
    enum Logic {
        LOGIC_ALL,
        LOGIC_ANY,
    };
    enum InstanceMode {
        INSTANCE_ANY,
        INSTANCE_EVERY,
        INSTANCE_TOTAL,
    };
    enum ChestMode {
        CHESTS_TOTAL,
        CHEST_ANY,
        CHEST_EVERY,
        CHEST_1,
        CHEST_2,
        CHEST_3,
        CHEST_4,
    };

    int structureType = Desert_Pyramid;
    int logic = LOGIC_ALL;
    int instanceMode = INSTANCE_ANY;
    int chestMode = CHESTS_TOTAL;
    QVector<LootRule> rules;

    bool isEmpty() const { return rules.isEmpty(); }
};

struct LootSearchCacheKey
{
    uint64_t rulesHash = 0;
    int mc = 0;
    int structureType = 0;
    int x = 0;
    int z = 0;
    int variant = 0;

    bool operator<(const LootSearchCacheKey& other) const;
};

struct LootSearchCacheEntry
{
    QVector<uint64_t> count[4];
    bool present[4] = {};
};

/**
 * Worker-local cache used by the optional 48-bit Loot precheck/speed mode.
 * Entries are discarded whenever the lower 48-bit seed changes.
 */
struct LootSearchCache
{
    uint64_t familySeed = ~(uint64_t)0;
    uint64_t calculations = 0;
    uint64_t hits = 0;
    std::map<LootSearchCacheKey, LootSearchCacheEntry> entries;
    std::map<const LootRuleSet*, uint64_t> ruleHashes;

    void reset();
};

bool isLootSupported(int structureType, int mc);
QString lootSupportDescription(int structureType, int mc);
QString validateLootRuleSet(const LootRuleSet& rules, int mc);

QByteArray serializeLootRuleSet(const LootRuleSet& rules);
bool deserializeLootRuleSet(
    const QByteArray& data, LootRuleSet *rules, QString *error = nullptr);

uint64_t registerLootRuleSet(const LootRuleSet& rules);
bool lookupLootRuleSet(uint64_t hash, LootRuleSet *rules);

bool matchStructureLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    Pos structurePos, int biomeId = -1,
    LootSearchCache *cache = nullptr,
    uint64_t cacheRuleKey = 0);
bool matchAreaLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    const QVector<Pos>& structurePositions,
    const QVector<int>& biomeIds = QVector<int>(),
    LootSearchCache *cache = nullptr,
    uint64_t cacheRuleKey = 0);

/**
 * Conservative lower-48-bit checks used before biome/viability is known.
 * Shipwrecks test both the ocean and beached outcomes. A true result means
 * that at least one outcome can match; false is safe to reject.
 */
bool canMatchStructureLoot48(
    const LootRuleSet& rules, int mc, uint64_t structureSeed,
    Pos structurePos, LootSearchCache *cache = nullptr,
    uint64_t cacheRuleKey = 0);
bool canMatchAreaLoot48(
    const LootRuleSet& rules, int mc, uint64_t structureSeed,
    const QVector<Pos>& candidatePositions, int minimumInstances,
    LootSearchCache *cache = nullptr,
    uint64_t cacheRuleKey = 0);

#endif // LOOTCONDITION_H
