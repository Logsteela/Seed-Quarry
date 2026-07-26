#include "villagelootseed.h"

#include "cubiomes/finders.h"
#include "cubiomes/noise.h"

#include <QHash>

#include <algorithm>

namespace {

qint64 chunkKey(int chunkX, int chunkZ)
{
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

int blockChunk(int coordinate)
{
    return floordiv(coordinate, 16);
}

bool pieceIntersectsChunk(
    const VillagePiece16& piece, int chunkX, int chunkZ)
{
    const int minX = chunkX * 16;
    const int minZ = chunkZ * 16;
    return piece.bb1.x >= minX && piece.bb0.x <= minX + 15 &&
        piece.bb1.z >= minZ && piece.bb0.z <= minZ + 15;
}

void advancePatchOffsets(
    uint64_t *random, int attempts,
    int xBound, int yBound, int zBound)
{
    for (int attempt = 0; attempt < attempts; attempt++)
    {
        nextInt(random, xBound);
        nextInt(random, xBound);
        nextInt(random, yBound);
        nextInt(random, yBound);
        nextInt(random, zBound);
        nextInt(random, zBound);
    }
}

const PerlinNoise& plainsFlowerNoise()
{
    static const PerlinNoise noise = [] {
        uint64_t random;
        setSeed(&random, UINT64_C(2345));
        PerlinNoise initialized = {};
        perlinInit(&initialized, &random);
        return initialized;
    }();
    return noise;
}

void advancePlainsFlower(
    uint64_t *random, const Pos3& position)
{
    const double noise = sampleSimplex2D(
        &plainsFlowerNoise(),
        double(position.x) / 200.0,
        double(position.z) / 200.0);
    if (noise < -0.8)
    {
        nextInt(random, 4);
    }
    else if (nextInt(random, 3) > 0)
    {
        nextInt(random, 4);
    }

    // DefaultFlowerFeature uses spread directly rather than spread + 1.
    advancePatchOffsets(random, 64, 7, 3, 7);
}

bool advanceSafeFeature(
    uint64_t *random, const VillagePiece16& piece)
{
    const QString& feature = piece.feature;
    if (feature.contains(
            QLatin1String("SWEET_BERRY_BUSH_CONFIG")))
    {
        // RandomPatchFeature uses spread + 1.
        advancePatchOffsets(random, 64, 8, 4, 8);
        return true;
    }
    if (feature.contains(
            QLatin1String("TAIGA_GRASS_CONFIG")))
    {
        // WeightedStateProvider has two entries and shuffles both.
        nextFloat(random);
        nextFloat(random);
        advancePatchOffsets(random, 32, 8, 4, 8);
        return true;
    }
    if (feature.contains(
            QLatin1String("PLAIN_FLOWER_CONFIG")))
    {
        advancePlainsFlower(random, piece.pos);
        return true;
    }
    return false;
}

}

bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChestSeed16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    bool anotherVillageMayReferenceAChestChunk,
    QString *error)
{
    if (!out)
    {
        if (error)
            *error = QStringLiteral("Village loot output is null.");
        return false;
    }
    out->clear();
    if (error)
        error->clear();

    QVector<QVector<int>> containersByPiece(
        layout.pieces.size());
    for (int index = 0;
         index < layout.containers.size(); index++)
    {
        const VillageContainer16& container =
            layout.containers[index];
        if (container.pieceIndex < 0 ||
            container.pieceIndex >= layout.pieces.size())
        {
            if (error)
            {
                *error = QStringLiteral(
                    "Village container has an invalid "
                    "piece index.");
            }
            out->clear();
            return false;
        }
        containersByPiece[container.pieceIndex].push_back(
            index);
    }
    for (QVector<int>& indices : containersByPiece)
    {
        std::sort(
            indices.begin(), indices.end(),
            [&layout](int first, int second) {
                const VillageContainer16& a =
                    layout.containers[first];
                const VillageContainer16& b =
                    layout.containers[second];
                if (a.placementIndex != b.placementIndex)
                {
                    return a.placementIndex <
                        b.placementIndex;
                }
                if (a.pos.y != b.pos.y)
                    return a.pos.y < b.pos.y;
                if (a.pos.x != b.pos.x)
                    return a.pos.x < b.pos.x;
                return a.pos.z < b.pos.z;
            });
    }

    QVector<int> outputByContainer(
        layout.containers.size(), -1);
    QHash<qint64, QVector<int>> targetChunks;
    for (int containerIndex = 0;
         containerIndex < layout.containers.size();
         containerIndex++)
    {
        const VillageContainer16& container =
            layout.containers[containerIndex];
        if (container.lootTable.isEmpty())
            continue;

        VillageLootChestSeed16 generated;
        generated.container = container;
        if (anotherVillageMayReferenceAChestChunk)
        {
            generated.quality =
                VILLAGE_LOOT_SEED_UNRESOLVED_OVERLAP;
        }
        const int outputIndex = out->size();
        out->push_back(generated);
        outputByContainer[containerIndex] = outputIndex;
        targetChunks[chunkKey(
            blockChunk(container.pos.x),
            blockChunk(container.pos.z))].push_back(
                outputIndex);
    }

    if (anotherVillageMayReferenceAChestChunk)
        return true;

    QVector<bool> assigned(out->size(), false);
    for (auto target = targetChunks.constBegin();
         target != targetChunks.constEnd(); ++target)
    {
        const int chunkX = chunkXFromKey(target.key());
        const int chunkZ = chunkZFromKey(target.key());
        uint64_t random;
        setSeed(
            &random,
            getPopulationSeed(
                MC_1_16_1, worldSeed,
                chunkX * 16, chunkZ * 16) +
                UINT64_C(40011));

        bool unresolved = false;
        for (int pieceIndex = 0;
             pieceIndex < layout.pieces.size();
             pieceIndex++)
        {
            const VillagePiece16& piece =
                layout.pieces[pieceIndex];
            if (!pieceIntersectsChunk(
                    piece, chunkX, chunkZ))
            {
                continue;
            }

            if (piece.elementType ==
                VillagePiece16::FEATURE)
            {
                if (!unresolved &&
                    !advanceSafeFeature(&random, piece))
                {
                    unresolved = true;
                }
                continue;
            }

            for (int containerIndex :
                 containersByPiece[pieceIndex])
            {
                const VillageContainer16& container =
                    layout.containers[containerIndex];
                if (blockChunk(container.pos.x) != chunkX ||
                    blockChunk(container.pos.z) != chunkZ)
                {
                    continue;
                }

                const int outputIndex =
                    outputByContainer[containerIndex];
                if (unresolved)
                {
                    if (outputIndex >= 0)
                    {
                        (*out)[outputIndex].quality =
                            VILLAGE_LOOT_SEED_UNRESOLVED_FEATURE;
                        assigned[outputIndex] = true;
                    }
                    continue;
                }

                const uint64_t lootTableSeed =
                    nextLong(&random);
                if (outputIndex >= 0)
                {
                    (*out)[outputIndex].lootTableSeed =
                        lootTableSeed;
                    (*out)[outputIndex].quality =
                        VILLAGE_LOOT_SEED_EXACT;
                    assigned[outputIndex] = true;
                }
            }
        }
    }
    for (bool wasAssigned : assigned)
    {
        if (wasAssigned)
            continue;
        if (error)
        {
            *error = QStringLiteral(
                "Village loot container was outside its "
                "own structure piece.");
        }
        out->clear();
        return false;
    }
    return true;
}
