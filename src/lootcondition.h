#ifndef LOOTCONDITION_H
#define LOOTCONDITION_H

#include "cubiomes/finders.h"
#include "cubiomes/loot.h"

#include <QByteArray>
#include <QString>
#include <QVector>

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

bool isLootSupported(int structureType, int mc);
QString lootSupportDescription(int structureType, int mc);
QString validateLootRuleSet(const LootRuleSet& rules, int mc);

QByteArray serializeLootRuleSet(const LootRuleSet& rules);
bool deserializeLootRuleSet(
    const QByteArray& data, LootRuleSet *rules, QString *error = nullptr);

uint64_t registerLootRuleSet(const LootRuleSet& rules);
bool lookupLootRuleSet(uint64_t hash, LootRuleSet *rules);

bool matchStructureLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed, Pos structurePos);
bool matchAreaLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    const QVector<Pos>& structurePositions);

#endif // LOOTCONDITION_H
