#include "lootcondition.h"

#include <QDataStream>
#include <QHash>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>

#include <limits>

namespace {

const quint32 LOOT_RULE_MAGIC = 0x31524c53; // "SLR1"
const quint16 LOOT_RULE_VERSION = 1;

QMutex g_lootRuleMutex;
QHash<quint64, QByteArray> g_lootRuleData;
QHash<quint64, LootRuleSet> g_lootRuleSets;

struct LootAccumulator
{
    quint64 count[DP_LOOT_ITEM_COUNT] = {};
    quint64 enchantedBook[DP_ENCH_COUNT][DP_ENCH_MAX_LEVEL + 1] = {};
};

struct LootChestSet
{
    StructureLoot chest[4] = {};
    bool present[4] = {};
};

quint64 contentHash(const QByteArray& data)
{
    quint64 hash = UINT64_C(14695981039346656037);
    for (unsigned char c : data)
    {
        hash ^= c;
        hash *= UINT64_C(1099511628211);
    }
    return hash ? hash : 1;
}

void addLoot(LootAccumulator *dst, const DesertPyramidLoot& src)
{
    for (int item = 0; item < DP_LOOT_ITEM_COUNT; item++)
        dst->count[item] += src.count[item];
    for (int ench = 0; ench < DP_ENCH_COUNT; ench++)
        for (int level = 1; level <= DP_ENCH_MAX_LEVEL; level++)
            dst->enchantedBook[ench][level] +=
                src.enchantedBook[ench][level];
}

quint64 countRule(const LootAccumulator& loot, const LootRule& rule)
{
    if (rule.item != DP_LOOT_ENCHANTED_BOOK || rule.enchantment < 0)
        return loot.count[rule.item];

    quint64 count = 0;
    int low = qMax(1, rule.minLevel);
    int high = qMin(DP_ENCH_MAX_LEVEL, rule.maxLevel);
    for (int level = low; level <= high; level++)
        count += loot.enchantedBook[rule.enchantment][level];
    return count;
}

bool matchesRules(const LootRuleSet& rules, const LootAccumulator& loot)
{
    if (rules.rules.isEmpty())
        return true;

    bool result = rules.logic == LootRuleSet::LOGIC_ALL;
    for (const LootRule& rule : rules.rules)
    {
        quint64 count = countRule(loot, rule);
        bool match = count >= quint64(rule.minCount) &&
            (rule.maxCount < 0 || count <= quint64(rule.maxCount));
        if (rules.logic == LootRuleSet::LOGIC_ALL)
        {
            if (!match)
                return false;
        }
        else if (match)
        {
            return true;
        }
        result = match;
    }
    return result;
}

bool matchesRules(
    const LootRuleSet& rules, const DesertPyramidLoot& loot)
{
    LootAccumulator accumulated;
    addLoot(&accumulated, loot);
    return matchesRules(rules, accumulated);
}

bool getStructureLoot(
    LootChestSet *out, const LootRuleSet& rules,
    int mc, uint64_t worldSeed, Pos pos, int biomeId)
{
    if (!out || !isLootSupported(rules.structureType, mc))
        return false;
    int chunkX = pos.x >> 4;
    int chunkZ = pos.z >> 4;
    if (rules.structureType == Desert_Pyramid)
    {
        for (int i = 0; i < 4; i++)
        {
            if (!getDesertPyramidLoot16(
                    &out->chest[i], worldSeed, chunkX, chunkZ, i))
                return false;
            out->present[i] = true;
        }
        return true;
    }
    if (rules.structureType == Treasure)
    {
        out->present[0] = getBuriedTreasureLoot16(
            &out->chest[0], worldSeed, chunkX, chunkZ);
        return out->present[0];
    }
    if (rules.structureType == Ruined_Portal ||
        rules.structureType == Ruined_Portal_N)
    {
        out->present[0] = getRuinedPortalLoot16(
            &out->chest[0], worldSeed, chunkX, chunkZ);
        return out->present[0];
    }
    if (rules.structureType == Shipwreck)
    {
        if (biomeId < 0)
            return false;
        uint8_t present[SHIPWRECK_CHEST_COUNT] = {};
        if (!getShipwreckLoot16(
                out->chest, present, worldSeed, chunkX, chunkZ,
                biomeId == beach || biomeId == snowy_beach))
            return false;
        for (int i = 0; i < SHIPWRECK_CHEST_COUNT; i++)
            out->present[i] = present[i];
        return true;
    }
    return false;
}

}

bool isLootSupported(int structureType, int mc)
{
    bool structureSupported =
        structureType == Desert_Pyramid ||
        structureType == Shipwreck ||
        structureType == Treasure ||
        structureType == Ruined_Portal ||
        structureType == Ruined_Portal_N;
    return structureSupported &&
        (mc == MC_1_16_1 || mc == MC_1_16_5);
}

QString lootSupportDescription(int structureType, int mc)
{
    if (isLootSupported(structureType, mc))
        return QString();
    if (structureType != Desert_Pyramid &&
        structureType != Shipwreck &&
        structureType != Treasure &&
        structureType != Ruined_Portal &&
        structureType != Ruined_Portal_N)
    {
        return QString::fromUtf8(
            "この構造物のチェスト内容計算にはまだ対応していません。");
    }
    return QString::fromUtf8(
        "このチェスト検索は Java 1.16.1 / 1.16.5 専用です。");
}

QString validateLootRuleSet(const LootRuleSet& rules, int mc)
{
    QString unsupported = lootSupportDescription(rules.structureType, mc);
    if (!unsupported.isEmpty())
        return unsupported;
    if (rules.logic < LootRuleSet::LOGIC_ALL ||
        rules.logic > LootRuleSet::LOGIC_ANY)
        return QString::fromUtf8("AND/OR の指定が不正です。");
    if (rules.instanceMode < LootRuleSet::INSTANCE_ANY ||
        rules.instanceMode > LootRuleSet::INSTANCE_TOTAL)
        return QString::fromUtf8("構造物の集計方法が不正です。");
    if (rules.chestMode < LootRuleSet::CHESTS_TOTAL ||
        rules.chestMode > LootRuleSet::CHEST_4)
        return QString::fromUtf8("チェストの集計方法が不正です。");
    if ((rules.structureType == Treasure ||
         rules.structureType == Ruined_Portal ||
         rules.structureType == Ruined_Portal_N) &&
        rules.chestMode > LootRuleSet::CHEST_1)
    {
        return QString::fromUtf8(
            "この構造物には指定したチェストがありません。");
    }
    if (rules.structureType == Shipwreck &&
        rules.chestMode > LootRuleSet::CHEST_3)
    {
        return QString::fromUtf8(
            "難破船には4番目のチェスト種別がありません。");
    }
    if (rules.rules.isEmpty())
        return QString::fromUtf8("アイテム条件を1個以上追加してください。");

    for (const LootRule& rule : rules.rules)
    {
        if (rule.item < 0 || rule.item >= DP_LOOT_ITEM_COUNT)
            return QString::fromUtf8("アイテムの指定が不正です。");
        if (!structureLootItemAvailable(
                rules.structureType, rule.item))
        {
            return QString::fromUtf8(
                "この構造物のチェストには指定したアイテムがありません。");
        }
        if (rule.minCount < 0 || rule.maxCount < -1 ||
            (rule.maxCount >= 0 && rule.minCount > rule.maxCount))
            return QString::fromUtf8("アイテム数の範囲が不正です。");
        if (rule.enchantment < -1 ||
            rule.enchantment >= DP_ENCH_COUNT)
            return QString::fromUtf8("エンチャントの指定が不正です。");
        if (rule.item == DP_LOOT_ENCHANTED_BOOK &&
            rule.enchantment >= 0 &&
            (rule.minLevel < 1 ||
             rule.maxLevel > desertPyramidEnchantmentMaxLevel(
                 rule.enchantment) ||
             rule.minLevel > rule.maxLevel))
        {
            return QString::fromUtf8("エンチャントレベルの範囲が不正です。");
        }
    }
    return QString();
}

QByteArray serializeLootRuleSet(const LootRuleSet& rules)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setVersion(QDataStream::Qt_5_15);
    stream << LOOT_RULE_MAGIC << LOOT_RULE_VERSION
           << qint16(rules.structureType)
           << quint8(rules.logic)
           << quint8(rules.instanceMode)
           << quint8(rules.chestMode)
           << quint32(rules.rules.size());
    for (const LootRule& rule : rules.rules)
    {
        stream << qint16(rule.item)
               << qint32(rule.minCount)
               << qint32(rule.maxCount)
               << qint16(rule.enchantment)
               << qint16(rule.minLevel)
               << qint16(rule.maxLevel);
    }
    return data;
}

bool deserializeLootRuleSet(
    const QByteArray& data, LootRuleSet *rules, QString *error)
{
    if (!rules)
        return false;
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setVersion(QDataStream::Qt_5_15);

    quint32 magic, count;
    quint16 version;
    qint16 structureType;
    quint8 logic, instanceMode, chestMode;
    stream >> magic >> version >> structureType
           >> logic >> instanceMode >> chestMode >> count;
    if (stream.status() != QDataStream::Ok ||
        magic != LOOT_RULE_MAGIC || version != LOOT_RULE_VERSION ||
        count > 100000)
    {
        if (error)
            *error = QString::fromUtf8("Loot条件データの形式が不正です。");
        return false;
    }

    LootRuleSet decoded;
    decoded.structureType = structureType;
    decoded.logic = logic;
    decoded.instanceMode = instanceMode;
    decoded.chestMode = chestMode;
    decoded.rules.reserve(count);
    for (quint32 i = 0; i < count; i++)
    {
        qint16 item, enchantment, minLevel, maxLevel;
        qint32 minCount, maxCount;
        stream >> item >> minCount >> maxCount
               >> enchantment >> minLevel >> maxLevel;
        if (stream.status() != QDataStream::Ok)
        {
            if (error)
                *error = QString::fromUtf8("Loot条件データが途中で切れています。");
            return false;
        }
        LootRule rule;
        rule.item = item;
        rule.minCount = minCount;
        rule.maxCount = maxCount;
        rule.enchantment = enchantment;
        rule.minLevel = minLevel;
        rule.maxLevel = maxLevel;
        decoded.rules.push_back(rule);
    }
    if (!stream.atEnd())
    {
        if (error)
            *error = QString::fromUtf8("Loot条件データの末尾に不明なデータがあります。");
        return false;
    }
    *rules = decoded;
    return true;
}

uint64_t registerLootRuleSet(const LootRuleSet& rules)
{
    QByteArray data = serializeLootRuleSet(rules);
    quint64 hash = contentHash(data);
    QMutexLocker locker(&g_lootRuleMutex);
    while (g_lootRuleData.contains(hash) && g_lootRuleData.value(hash) != data)
        hash++;
    g_lootRuleData[hash] = data;
    g_lootRuleSets[hash] = rules;
    return hash;
}

bool lookupLootRuleSet(uint64_t hash, LootRuleSet *rules)
{
    QMutexLocker locker(&g_lootRuleMutex);
    auto it = g_lootRuleSets.constFind(hash);
    if (it == g_lootRuleSets.constEnd())
        return false;
    if (rules)
        *rules = it.value();
    return true;
}

bool matchStructureLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    Pos structurePos, int biomeId)
{
    LootChestSet loots;
    if (!getStructureLoot(
            &loots, rules, mc, worldSeed, structurePos, biomeId))
        return false;

    if (rules.chestMode >= LootRuleSet::CHEST_1)
    {
        int index = rules.chestMode - LootRuleSet::CHEST_1;
        return index >= 0 && index < 4 && loots.present[index] &&
            matchesRules(rules, loots.chest[index]);
    }
    if (rules.chestMode == LootRuleSet::CHEST_ANY)
    {
        for (int i = 0; i < 4; i++)
            if (loots.present[i] &&
                matchesRules(rules, loots.chest[i]))
                return true;
        return false;
    }
    if (rules.chestMode == LootRuleSet::CHEST_EVERY)
    {
        bool found = false;
        for (int i = 0; i < 4; i++)
        {
            if (!loots.present[i])
                continue;
            found = true;
            if (!matchesRules(rules, loots.chest[i]))
                return false;
        }
        return found;
    }

    LootAccumulator total;
    for (int i = 0; i < 4; i++)
        if (loots.present[i])
            addLoot(&total, loots.chest[i]);
    return matchesRules(rules, total);
}

bool matchAreaLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    const QVector<Pos>& structurePositions,
    const QVector<int>& biomeIds)
{
    if (!biomeIds.isEmpty() &&
        biomeIds.size() != structurePositions.size())
        return false;
    LootAccumulator total;
    for (int position = 0;
         position < structurePositions.size(); position++)
    {
        LootChestSet loots;
        int biomeId = biomeIds.isEmpty() ? -1 : biomeIds[position];
        if (!getStructureLoot(
                &loots, rules, mc, worldSeed,
                structurePositions[position], biomeId))
            return false;
        for (int i = 0; i < 4; i++)
            if (loots.present[i])
                addLoot(&total, loots.chest[i]);
    }
    return matchesRules(rules, total);
}
