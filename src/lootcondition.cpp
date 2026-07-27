#include "lootcondition.h"

#include "bastionstructure.h"
#include "villagelootseed.h"
#include "villagestructure.h"

#include <QDataStream>
#include <QHash>
#include <QIODevice>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {

const quint32 LOOT_RULE_MAGIC = 0x31524c53; // "SLR1"
const quint16 LOOT_RULE_VERSION_LEGACY = 1;
const quint16 LOOT_RULE_VERSION_POSITION = 2;

QMutex g_lootRuleMutex;
QHash<quint64, QByteArray> g_lootRuleData;
QHash<quint64, LootRuleSet> g_lootRuleSets;

struct LootAccumulator
{
    quint64 count[DP_LOOT_ITEM_COUNT] = {};
    quint64 enchantedBook[DP_ENCH_COUNT][DP_ENCH_MAX_LEVEL + 1] = {};
};

struct GeneratedLootChest
{
    StructureLoot loot = {};
    bool present = false;
    bool contentsKnown = true;
    Pos3 pos = {};
    int table = -1;
    QString piece;
};

struct LootChestSet
{
    QVector<GeneratedLootChest> chests;
};

qint64 blockChunkKey(int blockX, int blockZ)
{
    const int chunkX = floordiv(blockX, 16);
    const int chunkZ = floordiv(blockZ, 16);
    return qint64(
        (quint64(quint32(chunkX)) << 32) |
        quint64(quint32(chunkZ)));
}

int chunkXFromKey(qint64 key)
{
    return qint32(quint64(key) >> 32);
}

int chunkZFromKey(qint64 key)
{
    return qint32(quint32(key));
}

bool hasVillageItemContentRule(const LootRuleSet& rules)
{
    for (const LootRule& rule : rules.rules)
    {
        if (rule.item != DP_LOOT_ANY_CONTAINER)
            return true;
    }
    return false;
}

QVector<Pos> villageLootChunksWithAnotherRngStart(
    const VillageLayout16& target, uint64_t worldSeed,
    int targetStartChunkX, int targetStartChunkZ)
{
    QSet<qint64> chestChunks;
    for (const VillageContainer16& container :
         target.containers)
    {
        if (!container.lootTable.isEmpty())
        {
            chestChunks.insert(blockChunkKey(
                container.pos.x, container.pos.z));
        }
    }
    if (chestChunks.isEmpty())
        return {};

    QSet<qint64> candidateStarts;
    for (qint64 chestKey : chestChunks)
    {
        const int chestChunkX = chunkXFromKey(chestKey);
        const int chestChunkZ = chunkZFromKey(chestKey);
        const int minRegionX =
            floordiv(chestChunkX - 8, 32);
        const int maxRegionX =
            floordiv(chestChunkX + 8, 32);
        const int minRegionZ =
            floordiv(chestChunkZ - 8, 32);
        const int maxRegionZ =
            floordiv(chestChunkZ + 8, 32);
        for (int regionZ = minRegionZ;
             regionZ <= maxRegionZ; regionZ++)
        {
            for (int regionX = minRegionX;
                 regionX <= maxRegionX; regionX++)
            {
                Pos start;
                if (!getStructurePos(
                        Village, MC_1_16_1, worldSeed,
                        regionX, regionZ, &start))
                {
                    continue;
                }
                const int startChunkX =
                    floordiv(start.x, 16);
                const int startChunkZ =
                    floordiv(start.z, 16);
                if (startChunkX == targetStartChunkX &&
                    startChunkZ == targetStartChunkZ)
                {
                    continue;
                }
                candidateStarts.insert(qint64(
                    (quint64(quint32(startChunkX)) << 32) |
                    quint64(quint32(startChunkZ))));
            }
        }
    }

    Generator generator;
    setupGenerator(&generator, MC_1_16_1, 0);
    applySeed(&generator, DIM_OVERWORLD, worldSeed);
    QSet<qint64> affectedChunks;
    for (qint64 startKey : candidateStarts)
    {
        const int startChunkX =
            chunkXFromKey(startKey);
        const int startChunkZ =
            chunkZFromKey(startKey);

        QSet<qint64> nearbyChestChunks;
        for (qint64 chestKey : chestChunks)
        {
            if (qAbs(startChunkX -
                     chunkXFromKey(chestKey)) <= 8 &&
                qAbs(startChunkZ -
                     chunkZFromKey(chestKey)) <= 8)
            {
                nearbyChestChunks.insert(chestKey);
            }
        }
        if (nearbyChestChunks.isEmpty())
            continue;

        const Pos start = {
            startChunkX * 16, startChunkZ * 16,
        };
        const int biomeId = isViableStructurePos(
            Village, &generator, start.x, start.z, 0);
        if (!biomeId)
            continue;

        VillageLayout16 neighbor;
        if (!generateVillageLayout16(
                &neighbor, worldSeed, startChunkX,
                startChunkZ, biomeId))
        {
            // A failed proof must not be treated as a safe single-start
            // placement.
            affectedChunks.unite(nearbyChestChunks);
            if (affectedChunks.size() == chestChunks.size())
                break;
            continue;
        }

        for (const VillagePiece16& piece : neighbor.pieces)
        {
            if (piece.elementType != VillagePiece16::FEATURE)
                continue;
            const qint64 pieceChunk = blockChunkKey(
                piece.pos.x, piece.pos.z);
            if (chestChunks.contains(pieceChunk))
                affectedChunks.insert(pieceChunk);
        }
        for (const VillageContainer16& container :
             neighbor.containers)
        {
            const qint64 containerChunk = blockChunkKey(
                container.pos.x, container.pos.z);
            if (chestChunks.contains(containerChunk))
                affectedChunks.insert(containerChunk);
        }
        if (affectedChunks.size() == chestChunks.size())
            break;
    }

    QVector<Pos> result;
    result.reserve(affectedChunks.size());
    for (qint64 chunkKey : affectedChunks)
    {
        result.push_back(Pos{
            chunkXFromKey(chunkKey),
            chunkZFromKey(chunkKey)});
    }
    return result;
}

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
    if (rule.item == DP_LOOT_ANY_CONTAINER)
        return 1;
    if (rule.item != DP_LOOT_ENCHANTED_BOOK || rule.enchantment < 0)
        return loot.count[rule.item];

    quint64 count = 0;
    int low = qMax(1, rule.minLevel);
    int high = qMin(DP_ENCH_MAX_LEVEL, rule.maxLevel);
    for (int level = low; level <= high; level++)
        count += loot.enchantedBook[rule.enchantment][level];
    return count;
}

LootMatchStatus matchesCountsStatus(
    const LootRuleSet& rules, const QVector<uint64_t>& counts,
    const QVector<bool>& known)
{
    if (rules.rules.isEmpty())
        return LOOT_MATCH_YES;

    bool sawUnknown = false;
    for (int i = 0; i < rules.rules.size(); i++)
    {
        const LootRule& rule = rules.rules[i];
        uint64_t count = i < counts.size() ? counts[i] : 0;
        const bool countKnown = i < known.size() ? known[i] : true;
        LootMatchStatus status;
        if (countKnown)
        {
            const bool match = count >= quint64(rule.minCount) &&
                (rule.maxCount < 0 ||
                 count <= quint64(rule.maxCount));
            status = match ? LOOT_MATCH_YES : LOOT_MATCH_NO;
        }
        else if (rule.maxCount >= 0 &&
                 count > quint64(rule.maxCount))
        {
            // Unknown containers can only add non-negative item counts.
            status = LOOT_MATCH_NO;
        }
        else if (rule.maxCount < 0 &&
                 count >= quint64(rule.minCount))
        {
            // With no upper bound, already meeting the minimum cannot be
            // invalidated by the unresolved containers.
            status = LOOT_MATCH_YES;
        }
        else
        {
            status = LOOT_MATCH_UNKNOWN;
        }

        if (rules.logic == LootRuleSet::LOGIC_ALL)
        {
            if (status == LOOT_MATCH_NO)
                return LOOT_MATCH_NO;
        }
        else if (status == LOOT_MATCH_YES)
        {
            return LOOT_MATCH_YES;
        }
        sawUnknown = sawUnknown ||
            status == LOOT_MATCH_UNKNOWN;
    }
    if (sawUnknown)
        return LOOT_MATCH_UNKNOWN;
    return rules.logic == LootRuleSet::LOGIC_ALL
        ? LOOT_MATCH_YES : LOOT_MATCH_NO;
}

LootMatchStatus matchesChest(
    const LootRuleSet& rules, const LootSearchCacheChest& chest)
{
    if (!chest.present)
        return LOOT_MATCH_NO;
    QVector<bool> known;
    known.reserve(rules.rules.size());
    for (const LootRule& rule : rules.rules)
    {
        known.push_back(
            chest.contentsKnown ||
            rule.item == DP_LOOT_ANY_CONTAINER);
    }
    return matchesCountsStatus(rules, chest.counts, known);
}

QVector<uint64_t> getRuleCounts(
    const LootRuleSet& rules, const StructureLoot& loot)
{
    LootAccumulator accumulated;
    addLoot(&accumulated, loot);
    QVector<uint64_t> counts;
    counts.reserve(rules.rules.size());
    for (const LootRule& rule : rules.rules)
        counts.push_back(countRule(accumulated, rule));
    return counts;
}

bool chestPositionMatches(
    const LootRuleSet& rules, const GeneratedLootChest& chest,
    Pos structurePos)
{
    if (rules.chestPositionMode ==
        LootRuleSet::CHEST_POSITION_ANY)
    {
        return true;
    }
    if (rules.structureType != Bastion &&
        rules.structureType != Village)
        return false;

    int x = chest.pos.x;
    int y = chest.pos.y;
    int z = chest.pos.z;
    if (rules.chestPositionMode ==
        LootRuleSet::CHEST_POSITION_RELATIVE)
    {
        if (rules.structureType != Bastion)
            return false;
        x -= (structurePos.x >> 4) << 4;
        y -= 32;
        z -= (structurePos.z >> 4) << 4;
    }
    return x >= rules.chestMinX && x <= rules.chestMaxX &&
        y >= rules.chestMinY && y <= rules.chestMaxY &&
        z >= rules.chestMinZ && z <= rules.chestMaxZ;
}

bool getStructureLoot(
    LootChestSet *out, const LootRuleSet& rules,
    int mc, uint64_t worldSeed, Pos pos, int biomeId)
{
    if (!out || !isLootSupported(rules.structureType, mc))
        return false;
    out->chests.clear();

    int chunkX = pos.x >> 4;
    int chunkZ = pos.z >> 4;
    if (rules.structureType == Desert_Pyramid)
    {
        out->chests.resize(4);
        for (int i = 0; i < out->chests.size(); i++)
        {
            GeneratedLootChest& chest = out->chests[i];
            if (!getDesertPyramidLoot16(
                    &chest.loot, worldSeed, chunkX, chunkZ, i))
                return false;
            chest.present = true;
        }
        return true;
    }
    if (rules.structureType == Treasure)
    {
        out->chests.resize(1);
        GeneratedLootChest& chest = out->chests[0];
        chest.present = getBuriedTreasureLoot16(
            &chest.loot, worldSeed, chunkX, chunkZ);
        return chest.present;
    }
    if (rules.structureType == Ruined_Portal ||
        rules.structureType == Ruined_Portal_N)
    {
        out->chests.resize(1);
        GeneratedLootChest& chest = out->chests[0];
        chest.present = getRuinedPortalLoot16(
            &chest.loot, worldSeed, chunkX, chunkZ);
        return chest.present;
    }
    if (rules.structureType == Shipwreck)
    {
        if (biomeId < 0)
            return false;
        StructureLoot loot[SHIPWRECK_CHEST_COUNT] = {};
        uint8_t present[SHIPWRECK_CHEST_COUNT] = {};
        if (!getShipwreckLoot16(
                loot, present, worldSeed, chunkX, chunkZ,
                biomeId == beach || biomeId == snowy_beach))
            return false;
        out->chests.resize(SHIPWRECK_CHEST_COUNT);
        for (int i = 0; i < SHIPWRECK_CHEST_COUNT; i++)
        {
            out->chests[i].loot = loot[i];
            out->chests[i].present = present[i];
        }
        return true;
    }
    if (rules.structureType == Bastion)
    {
        BastionLayout16 layout;
        if (!generateBastionLayout16(
                &layout, worldSeed, chunkX, chunkZ))
            return false;
        out->chests.reserve(layout.chests.size());
        for (const BastionLootChest16& generated :
             layout.chests)
        {
            GeneratedLootChest chest;
            chest.present = generateStructureLootTable16(
                &chest.loot, generated.table,
                generated.lootTableSeed);
            if (!chest.present)
                return false;
            chest.pos = generated.pos;
            chest.table = generated.table;
            chest.piece = generated.piece;
            out->chests.push_back(chest);
        }
        return true;
    }
    if (rules.structureType == Village)
    {
        if (biomeId < 0)
            return false;
        VillageLayout16 layout;
        if (!generateVillageLayout16(
                &layout, worldSeed, chunkX, chunkZ, biomeId))
        {
            return false;
        }

        QVector<VillageLootChestSeed16> generatedChests;
        QVector<Pos> overlappingRngChunks;
        if (hasVillageItemContentRule(rules))
        {
            overlappingRngChunks =
                villageLootChunksWithAnotherRngStart(
                    layout, worldSeed, chunkX, chunkZ);
        }
        if (!assignVillageLootSeedsSingleStart16(
                &generatedChests, layout, worldSeed,
                overlappingRngChunks))
        {
            return false;
        }
        out->chests.reserve(generatedChests.size());
        for (const VillageLootChestSeed16& generated :
             generatedChests)
        {
            GeneratedLootChest chest;
            chest.present = true;
            chest.contentsKnown = generated.isExact();
            if (chest.contentsKnown &&
                !generateStructureLootTable16(
                    &chest.loot, generated.container.table,
                    generated.lootTableSeed))
            {
                return false;
            }
            chest.pos = generated.container.pos;
            chest.table = generated.container.table;
            chest.piece = generated.container.piece;
            out->chests.push_back(chest);
        }
        return true;
    }
    return false;
}

bool getCachedRuleCounts(
    LootSearchCacheEntry *out, const LootRuleSet& rules,
    int mc, uint64_t worldSeed, Pos pos, int biomeId,
    LootSearchCache *cache, uint64_t cacheRuleKey)
{
    if (!out)
        return false;

    // Village terrain, Jigsaw collisions, and therefore its chest list can
    // change between the 65536 full seeds in one lower-48 family.
    if (rules.structureType == Village)
        cache = nullptr;

    LootSearchCacheKey key;
    if (cache)
    {
        const uint64_t familySeed = worldSeed & MASK48;
        if (cache->familySeed != familySeed)
        {
            cache->familySeed = familySeed;
            cache->entries.clear();
        }
        if (cacheRuleKey)
        {
            key.rulesHash = cacheRuleKey;
        }
        else
        {
            auto ruleHash = cache->ruleHashes.find(&rules);
            if (ruleHash == cache->ruleHashes.end())
            {
                ruleHash = cache->ruleHashes.emplace(
                    &rules,
                    contentHash(serializeLootRuleSet(rules))).first;
            }
            key.rulesHash = ruleHash->second;
        }
        key.mc = mc;
        key.structureType = rules.structureType;
        key.x = pos.x;
        key.z = pos.z;
        // Shipwreck templates and the decorator RNG differ between the
        // beached and ocean families. Other supported Loot tables do not
        // depend on their biome/structure variant.
        key.variant = rules.structureType == Shipwreck &&
            (biomeId == beach || biomeId == snowy_beach);

        auto found = cache->entries.find(key);
        if (found != cache->entries.end())
        {
            cache->hits++;
            *out = found->second;
            return true;
        }
        cache->calculations++;
    }

    LootChestSet loots;
    if (!getStructureLoot(
            &loots, rules, mc, worldSeed, pos, biomeId))
        return false;

    LootSearchCacheEntry generated;
    generated.chests.reserve(loots.chests.size());
    for (const GeneratedLootChest& lootChest : loots.chests)
    {
        LootSearchCacheChest cachedChest;
        cachedChest.present = lootChest.present &&
            chestPositionMatches(rules, lootChest, pos);
        cachedChest.contentsKnown = lootChest.contentsKnown;
        cachedChest.pos = lootChest.pos;
        cachedChest.table = lootChest.table;
        cachedChest.piece = lootChest.piece;
        if (lootChest.present)
            cachedChest.counts =
                getRuleCounts(rules, lootChest.loot);
        generated.chests.push_back(cachedChest);
    }
    *out = generated;
    if (cache)
        cache->entries[key] = generated;
    return true;
}

QVector<uint64_t> totalRuleCounts(
    const LootRuleSet& rules, const LootSearchCacheEntry& entry,
    QVector<bool> *known)
{
    const int ruleCount = rules.rules.size();
    QVector<uint64_t> total(ruleCount, 0);
    if (known)
        known->fill(true, ruleCount);
    for (const LootSearchCacheChest& chest : entry.chests)
    {
        if (!chest.present)
            continue;
        for (int rule = 0;
             rule < ruleCount && rule < chest.counts.size();
             rule++)
        {
            total[rule] += chest.counts[rule];
            if (known && !chest.contentsKnown &&
                rules.rules[rule].item != DP_LOOT_ANY_CONTAINER)
            {
                (*known)[rule] = false;
            }
        }
    }
    return total;
}

bool rangeCanMatch(
    const LootRule& rule, uint64_t minimum, uint64_t maximum)
{
    return maximum >= uint64_t(rule.minCount) &&
        (rule.maxCount < 0 || minimum <= uint64_t(rule.maxCount));
}

}

bool LootSearchCacheKey::operator<(
    const LootSearchCacheKey& other) const
{
    if (rulesHash != other.rulesHash)
        return rulesHash < other.rulesHash;
    if (mc != other.mc)
        return mc < other.mc;
    if (structureType != other.structureType)
        return structureType < other.structureType;
    if (x != other.x)
        return x < other.x;
    if (z != other.z)
        return z < other.z;
    return variant < other.variant;
}

void LootSearchCache::reset()
{
    familySeed = ~(uint64_t)0;
    calculations = 0;
    hits = 0;
    entries.clear();
    ruleHashes.clear();
}

bool isLootSupported(int structureType, int mc)
{
    const bool fixedStructureSupported =
        structureType == Desert_Pyramid ||
        structureType == Shipwreck ||
        structureType == Treasure ||
        structureType == Ruined_Portal ||
        structureType == Ruined_Portal_N;
    if (fixedStructureSupported)
        return mc == MC_1_16_1 || mc == MC_1_16_5;
    if (structureType == Bastion)
        return mc == MC_1_16_1 &&
            isBastionStructureData16Available();
    if (structureType == Village)
        return mc == MC_1_16_1 &&
            isVillageStructureData16Available();
    return false;
}

QString lootSupportDescription(int structureType, int mc)
{
    if (isLootSupported(structureType, mc))
        return QString();
    if (structureType == Bastion)
    {
        if (mc != MC_1_16_1)
        {
            return QString::fromUtf8(
                "砦の遺跡の正確なチェスト検索は、現在Java 1.16.1専用です。");
        }
        QString error;
        isBastionStructureData16Available(&error);
        return error;
    }
    if (structureType == Village)
    {
        if (mc != MC_1_16_1)
        {
            return QString::fromUtf8(
                "村の正確なピース・チェスト位置検索は、"
                "現在Java 1.16.1専用です。");
        }
        QString error;
        isVillageStructureData16Available(&error);
        return error;
    }
    if (structureType != Desert_Pyramid &&
        structureType != Shipwreck &&
        structureType != Treasure &&
        structureType != Ruined_Portal &&
        structureType != Ruined_Portal_N &&
        structureType != Bastion)
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
    if ((rules.structureType == Bastion ||
         rules.structureType == Village) &&
        rules.chestMode >= LootRuleSet::CHEST_1)
    {
        return QString::fromUtf8(
            "この構造物はコンテナ数が変動するため、"
            "合計・いずれか・各コンテナのいずれかを選んでください。");
    }
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
    if (rules.chestPositionMode <
            LootRuleSet::CHEST_POSITION_ANY ||
        rules.chestPositionMode >
            LootRuleSet::CHEST_POSITION_RELATIVE)
    {
        return QString::fromUtf8(
            "チェスト座標の指定方法が不正です。");
    }
    if (rules.chestPositionMode !=
            LootRuleSet::CHEST_POSITION_ANY &&
        rules.structureType != Bastion &&
        rules.structureType != Village)
    {
        return QString::fromUtf8(
            "チェスト座標による絞り込みは、現在は村と砦の遺跡に対応しています。");
    }
    if (rules.chestPositionMode ==
            LootRuleSet::CHEST_POSITION_RELATIVE &&
        rules.structureType != Bastion)
    {
        return QString::fromUtf8(
            "開始位置からの相対チェスト座標は、現在は砦の遺跡に対応しています。"
            "村ではワールド絶対座標を選んでください。");
    }
    if (rules.chestPositionMode !=
            LootRuleSet::CHEST_POSITION_ANY &&
        (rules.chestMinX > rules.chestMaxX ||
         rules.chestMinY > rules.chestMaxY ||
         rules.chestMinZ > rules.chestMaxZ))
    {
        return QString::fromUtf8(
            "チェスト座標の最小値が最大値を超えています。");
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
    const quint16 version =
        rules.chestPositionMode ==
            LootRuleSet::CHEST_POSITION_ANY
        ? LOOT_RULE_VERSION_LEGACY
        : LOOT_RULE_VERSION_POSITION;
    stream << LOOT_RULE_MAGIC << version
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
    if (version >= LOOT_RULE_VERSION_POSITION)
    {
        stream << quint8(rules.chestPositionMode)
               << qint32(rules.chestMinX)
               << qint32(rules.chestMaxX)
               << qint32(rules.chestMinY)
               << qint32(rules.chestMaxY)
               << qint32(rules.chestMinZ)
               << qint32(rules.chestMaxZ);
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
        magic != LOOT_RULE_MAGIC ||
        (version != LOOT_RULE_VERSION_LEGACY &&
         version != LOOT_RULE_VERSION_POSITION) ||
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
    if (version >= LOOT_RULE_VERSION_POSITION)
    {
        quint8 positionMode;
        qint32 minX, maxX, minY, maxY, minZ, maxZ;
        stream >> positionMode
               >> minX >> maxX
               >> minY >> maxY
               >> minZ >> maxZ;
        if (stream.status() != QDataStream::Ok)
        {
            if (error)
                *error = QString::fromUtf8(
                    "Loot条件のチェスト座標データが途中で切れています。");
            return false;
        }
        decoded.chestPositionMode = positionMode;
        decoded.chestMinX = minX;
        decoded.chestMaxX = maxX;
        decoded.chestMinY = minY;
        decoded.chestMaxY = maxY;
        decoded.chestMinZ = minZ;
        decoded.chestMaxZ = maxZ;
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

LootMatchStatus matchStructureLootStatus(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    Pos structurePos, int biomeId, LootSearchCache *cache,
    uint64_t cacheRuleKey)
{
    LootSearchCacheEntry counts;
    if (!getCachedRuleCounts(
            &counts, rules, mc, worldSeed,
            structurePos, biomeId, cache, cacheRuleKey))
        return LOOT_MATCH_NO;

    if (rules.chestMode >= LootRuleSet::CHEST_1)
    {
        int index = rules.chestMode - LootRuleSet::CHEST_1;
        if (index < 0 || index >= counts.chests.size())
            return LOOT_MATCH_NO;
        return matchesChest(rules, counts.chests[index]);
    }
    if (rules.chestMode == LootRuleSet::CHEST_ANY)
    {
        bool sawUnknown = false;
        for (const LootSearchCacheChest& chest : counts.chests)
        {
            const LootMatchStatus status =
                matchesChest(rules, chest);
            if (status == LOOT_MATCH_YES)
                return LOOT_MATCH_YES;
            sawUnknown = sawUnknown ||
                status == LOOT_MATCH_UNKNOWN;
        }
        return sawUnknown
            ? LOOT_MATCH_UNKNOWN : LOOT_MATCH_NO;
    }
    if (rules.chestMode == LootRuleSet::CHEST_EVERY)
    {
        bool found = false;
        bool sawUnknown = false;
        for (const LootSearchCacheChest& chest : counts.chests)
        {
            if (!chest.present)
                continue;
            found = true;
            const LootMatchStatus status =
                matchesChest(rules, chest);
            if (status == LOOT_MATCH_NO)
                return LOOT_MATCH_NO;
            sawUnknown = sawUnknown ||
                status == LOOT_MATCH_UNKNOWN;
        }
        if (!found)
            return LOOT_MATCH_NO;
        return sawUnknown
            ? LOOT_MATCH_UNKNOWN : LOOT_MATCH_YES;
    }

    QVector<bool> known;
    const QVector<uint64_t> total =
        totalRuleCounts(rules, counts, &known);
    return matchesCountsStatus(rules, total, known);
}

LootMatchStatus matchAreaLootStatus(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    const QVector<Pos>& structurePositions,
    const QVector<int>& biomeIds, LootSearchCache *cache,
    uint64_t cacheRuleKey)
{
    if (!biomeIds.isEmpty() &&
        biomeIds.size() != structurePositions.size())
        return LOOT_MATCH_NO;
    QVector<uint64_t> total(rules.rules.size(), 0);
    QVector<bool> known(rules.rules.size(), true);
    for (int position = 0;
         position < structurePositions.size(); position++)
    {
        LootSearchCacheEntry counts;
        int biomeId = biomeIds.isEmpty() ? -1 : biomeIds[position];
        if (!getCachedRuleCounts(
                &counts, rules, mc, worldSeed,
                structurePositions[position], biomeId, cache,
                cacheRuleKey))
            return LOOT_MATCH_NO;
        for (const LootSearchCacheChest& chest : counts.chests)
        {
            if (!chest.present)
                continue;
            for (int rule = 0; rule < total.size(); rule++)
            {
                total[rule] += chest.counts[rule];
                if (!chest.contentsKnown &&
                    rules.rules[rule].item !=
                        DP_LOOT_ANY_CONTAINER)
                {
                    known[rule] = false;
                }
            }
        }
    }
    return matchesCountsStatus(rules, total, known);
}

bool matchStructureLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    Pos structurePos, int biomeId, LootSearchCache *cache,
    uint64_t cacheRuleKey)
{
    return matchStructureLootStatus(
        rules, mc, worldSeed, structurePos, biomeId,
        cache, cacheRuleKey) == LOOT_MATCH_YES;
}

bool matchAreaLoot(
    const LootRuleSet& rules, int mc, uint64_t worldSeed,
    const QVector<Pos>& structurePositions,
    const QVector<int>& biomeIds, LootSearchCache *cache,
    uint64_t cacheRuleKey)
{
    return matchAreaLootStatus(
        rules, mc, worldSeed, structurePositions, biomeIds,
        cache, cacheRuleKey) == LOOT_MATCH_YES;
}

bool canMatchStructureLoot48(
    const LootRuleSet& rules, int mc, uint64_t structureSeed,
    Pos structurePos, LootSearchCache *cache,
    uint64_t cacheRuleKey)
{
    /*
     * A village's start pool and exact Y layout need the biome and the full
     * 64-bit terrain seed.  The lower-48 pass may therefore keep it as a
     * candidate, but must not reject it from an invented variant/layout.
     */
    if (rules.structureType == Village)
        return true;

    if (rules.structureType != Shipwreck)
    {
        return matchStructureLoot(
            rules, mc, structureSeed, structurePos, -1,
            cache, cacheRuleKey);
    }

    // A structure seed does not determine whether a shipwreck is beached.
    // Calculate both now so FULL_64 can select the real one from the cache
    // after biome viability is known.
    bool oceanMatch = matchStructureLoot(
        rules, mc, structureSeed, structurePos, ocean,
        cache, cacheRuleKey);
    bool beachedMatch = matchStructureLoot(
        rules, mc, structureSeed, structurePos, beach,
        cache, cacheRuleKey);
    return oceanMatch || beachedMatch;
}

bool canMatchAreaLoot48(
    const LootRuleSet& rules, int mc, uint64_t structureSeed,
    const QVector<Pos>& candidatePositions, int minimumInstances,
    LootSearchCache *cache, uint64_t cacheRuleKey)
{
    if (rules.rules.isEmpty())
        return true;
    minimumInstances = qMax(0, minimumInstances);
    if (candidatePositions.size() < minimumInstances)
        return false;
    if (rules.structureType == Village)
        return true;

    const int ruleCount = rules.rules.size();
    QVector<uint64_t> maximum(ruleCount, 0);
    QVector<QVector<uint64_t>> possibleMinimums(ruleCount);

    for (Pos pos : candidatePositions)
    {
        LootSearchCacheEntry first;
        if (!getCachedRuleCounts(
                &first, rules, mc, structureSeed, pos,
                rules.structureType == Shipwreck ? ocean : -1,
                cache, cacheRuleKey))
        {
            return false;
        }
        QVector<uint64_t> low =
            totalRuleCounts(rules, first, nullptr);
        QVector<uint64_t> high = low;

        if (rules.structureType == Shipwreck)
        {
            LootSearchCacheEntry beached;
            if (!getCachedRuleCounts(
                    &beached, rules, mc, structureSeed, pos, beach,
                    cache, cacheRuleKey))
            {
                return false;
            }
            QVector<uint64_t> other =
                totalRuleCounts(rules, beached, nullptr);
            for (int rule = 0; rule < ruleCount; rule++)
            {
                low[rule] = qMin(low[rule], other[rule]);
                high[rule] = qMax(high[rule], other[rule]);
            }
        }

        for (int rule = 0; rule < ruleCount; rule++)
        {
            possibleMinimums[rule].push_back(low[rule]);
            maximum[rule] += high[rule];
        }
    }

    bool anyPossible = false;
    for (int rule = 0; rule < ruleCount; rule++)
    {
        QVector<uint64_t>& values = possibleMinimums[rule];
        std::sort(values.begin(), values.end());
        uint64_t minimum = 0;
        for (int i = 0; i < minimumInstances; i++)
            minimum += values[i];

        bool possible =
            rangeCanMatch(rules.rules[rule], minimum, maximum[rule]);
        if (rules.logic == LootRuleSet::LOGIC_ALL && !possible)
            return false;
        if (possible)
            anyPossible = true;
    }
    return rules.logic == LootRuleSet::LOGIC_ALL || anyPossible;
}
