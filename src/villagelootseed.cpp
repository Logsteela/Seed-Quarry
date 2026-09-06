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

qint64 blockKey(const Pos3& pos);

const VillagePathBlock16 *villagePathAt(
    const VillageLayout16& layout, int pieceIndex,
    const Pos3& pos)
{
    const VillagePathBlock16 *result = nullptr;
    const auto paths = layout.grassPathsByPosition.constFind(
        blockKey(pos));
    if (paths == layout.grassPathsByPosition.constEnd())
        return nullptr;
    for (const VillagePathBlock16& path : *paths)
    {
        if (path.pieceIndex >= pieceIndex)
            continue;
        if (path.pos.x == pos.x &&
            path.pos.y == pos.y &&
            path.pos.z == pos.z &&
            (!result ||
             path.pieceIndex > result->pieceIndex))
        {
            result = &path;
        }
    }
    return result;
}

qint64 horizontalKey(int x, int z)
{
    return qint64(
        (quint64(quint32(x)) << 32) |
        quint64(quint32(z)));
}

qint64 blockKey(const Pos3& pos)
{
    return qint64(
        ((quint64(quint32(pos.x)) &
          UINT64_C(0x3ffffff)) << 38) |
        ((quint64(quint32(pos.z)) &
          UINT64_C(0x3ffffff)) << 12) |
        (quint64(quint32(pos.y)) & UINT64_C(0xfff)));
}

bool samePosition(const Pos3& first, const Pos3& second)
{
    return first.x == second.x &&
        first.y == second.y &&
        first.z == second.z;
}

enum FeatureBlockState
{
    FEATURE_BLOCK_UNKNOWN,
    FEATURE_BLOCK_AIR,
    FEATURE_BLOCK_PATH,
    FEATURE_BLOCK_STURDY,
    FEATURE_BLOCK_SAND,
    FEATURE_BLOCK_SOIL,
    FEATURE_BLOCK_TREE_FREE,
    FEATURE_BLOCK_TREE_FREE_SOLID,
    FEATURE_BLOCK_TREE_FREE_STURDY,
    FEATURE_BLOCK_WATER,
    FEATURE_BLOCK_OCCUPIED,
    FEATURE_BLOCK_PLACED,
};

struct FeatureResolution16
{
    int reason = VILLAGE_LOOT_UNRESOLVED_NONE;

    void fail(int value)
    {
        if (reason == VILLAGE_LOOT_UNRESOLVED_NONE)
            reason = value;
    }
};

FeatureBlockState featureBlockState(
    const VillageLayout16& layout, int pieceIndex,
    const QVector<Pos3>& placed, const Pos3& pos,
    FeatureResolution16 *resolution)
{
    for (const Pos3& block : placed)
    {
        if (samePosition(block, pos))
            return FEATURE_BLOCK_PLACED;
    }
    const Pos3& featureOrigin =
        layout.pieces[pieceIndex].pos;
    const bool insidePlacementChunk =
        blockChunk(pos.x) == blockChunk(featureOrigin.x) &&
        blockChunk(pos.z) == blockChunk(featureOrigin.z);

    if (insidePlacementChunk)
    {
        if (const VillagePathBlock16 *path =
            villagePathAt(layout, pieceIndex, pos))
        {
            if (!path->stateKnown)
            {
                resolution->fail(
                    VILLAGE_LOOT_UNRESOLVED_PATH_STATE);
                return FEATURE_BLOCK_UNKNOWN;
            }
            return path->isPath
                ? FEATURE_BLOCK_PATH
                : FEATURE_BLOCK_STURDY;
        }
        const auto blocks = layout.placedBlocks.constFind(
            blockKey(pos));
        if (blocks != layout.placedBlocks.constEnd())
        {
            const VillagePlacedBlock16 *latest = nullptr;
            for (const VillagePlacedBlock16& block : *blocks)
            {
                if (block.pieceIndex < pieceIndex &&
                    (!latest ||
                     block.pieceIndex > latest->pieceIndex))
                {
                    latest = &block;
                }
            }
            if (latest)
            {
                if (!latest->stateKnown)
                {
                    resolution->fail(
                        VILLAGE_LOOT_UNRESOLVED_TEMPLATE_STATE);
                    return FEATURE_BLOCK_UNKNOWN;
                }
                switch (latest->kind)
                {
                case VillagePlacedBlock16::STURDY:
                    return FEATURE_BLOCK_STURDY;
                case VillagePlacedBlock16::TREE_FREE:
                    return FEATURE_BLOCK_TREE_FREE;
                case VillagePlacedBlock16::TREE_FREE_SOLID:
                    return FEATURE_BLOCK_TREE_FREE_SOLID;
                case VillagePlacedBlock16::TREE_FREE_STURDY:
                    return FEATURE_BLOCK_TREE_FREE_STURDY;
                case VillagePlacedBlock16::SOIL:
                    return FEATURE_BLOCK_SOIL;
                case VillagePlacedBlock16::SAND:
                    return FEATURE_BLOCK_SAND;
                case VillagePlacedBlock16::WATER:
                    return FEATURE_BLOCK_WATER;
                default:
                    return FEATURE_BLOCK_OCCUPIED;
                }
            }
        }

      Pos3 below = pos;
      below.y--;
      const auto paths = layout.grassPathsByPosition.constFind(
          blockKey(below));
      if (paths != layout.grassPathsByPosition.constEnd())
      {
        for (const VillagePathBlock16& path : *paths)
        {
            if (path.pieceIndex >= pieceIndex)
                continue;
            if (!path.stateKnown)
            {
                resolution->fail(VILLAGE_LOOT_UNRESOLVED_PATH_STATE);
                return FEATURE_BLOCK_UNKNOWN;
            }
            if (path.aboveEmpty)
                return FEATURE_BLOCK_AIR;
            break;
        }
      }
    }

    const auto surface = layout.featureSurfaceHeights.constFind(
        horizontalKey(pos.x, pos.z));
    if (surface == layout.featureSurfaceHeights.constEnd())
    {
        resolution->fail(VILLAGE_LOOT_UNRESOLVED_SURFACE_MISSING);
        return FEATURE_BLOCK_UNKNOWN;
    }
    if (pos.y >= *surface)
        return FEATURE_BLOCK_AIR;
    if (pos.y == *surface - 1)
    {
        if (*surface == 63)
            return FEATURE_BLOCK_WATER;
        return layout.villageType == VillageLayout16::DESERT
            ? FEATURE_BLOCK_SAND
            : FEATURE_BLOCK_SOIL;
    }
    resolution->fail(VILLAGE_LOOT_UNRESOLVED_DEEP_TERRAIN);
    return FEATURE_BLOCK_UNKNOWN;
}

enum BlockPileProvider
{
    PILE_SIMPLE,
    PILE_ROTATED,
    PILE_WEIGHTED,
};

bool advanceBlockPile(
    uint64_t *random, const VillageLayout16& layout,
    int pieceIndex, const Pos3& origin,
    BlockPileProvider provider,
    FeatureResolution16 *resolution)
{
    if (origin.y < 5)
        return true;

    uint64_t advanced = *random;
    const int radiusX = 2 + nextInt(&advanced, 2);
    const int radiusZ = 2 + nextInt(&advanced, 2);
    QVector<Pos3> placed;
    for (int z = -radiusZ; z <= radiusZ; z++)
    {
        for (int y = 0; y <= 1; y++)
        {
            for (int x = -radiusX; x <= radiusX; x++)
            {
                const float threshold =
                    nextFloat(&advanced) * 10.0f -
                    nextFloat(&advanced) * 6.0f;
                bool triesPlacement =
                    float(x * x + z * z) <= threshold;
                if (!triesPlacement)
                {
                    triesPlacement =
                        double(nextFloat(&advanced)) < 0.031;
                }
                if (!triesPlacement)
                    continue;

                const Pos3 candidate = {
                    origin.x + x,
                    origin.y + y,
                    origin.z + z,
                };
                const FeatureBlockState candidateState =
                    featureBlockState(
                        layout, pieceIndex, placed, candidate,
                        resolution);
                if (candidateState == FEATURE_BLOCK_UNKNOWN)
                    return false;
                if (candidateState != FEATURE_BLOCK_AIR)
                    continue;

                Pos3 support = candidate;
                support.y--;
                const FeatureBlockState supportState =
                    featureBlockState(
                        layout, pieceIndex, placed, support,
                        resolution);
                if (supportState == FEATURE_BLOCK_UNKNOWN)
                    return false;

                bool mayPlace =
                    supportState == FEATURE_BLOCK_STURDY ||
                    supportState == FEATURE_BLOCK_SAND ||
                    supportState == FEATURE_BLOCK_SOIL ||
                    supportState ==
                        FEATURE_BLOCK_TREE_FREE_STURDY ||
                    supportState == FEATURE_BLOCK_PLACED;
                if (supportState == FEATURE_BLOCK_PATH)
                    mayPlace = next(&advanced, 1) != 0;
                if (!mayPlace)
                    continue;

                placed.push_back(candidate);
                if (provider == PILE_ROTATED)
                {
                    (void) nextInt(&advanced, 3);
                }
                else if (provider == PILE_WEIGHTED)
                {
                    (void) nextFloat(&advanced);
                    (void) nextFloat(&advanced);
                }
            }
        }
    }
    *random = advanced;
    return true;
}

bool advanceCactusPatch(
    uint64_t *random, const VillageLayout16& layout,
    int pieceIndex, const Pos3& origin,
    FeatureResolution16 *resolution)
{
    uint64_t advanced = *random;
    QVector<Pos3> cactus;
    for (int attempt = 0; attempt < 10; attempt++)
    {
        Pos3 candidate = {
            origin.x + nextInt(&advanced, 8) -
                nextInt(&advanced, 8),
            origin.y + nextInt(&advanced, 4) -
                nextInt(&advanced, 4),
            origin.z + nextInt(&advanced, 8) -
                nextInt(&advanced, 8),
        };
        const FeatureBlockState candidateState =
            featureBlockState(
                layout, pieceIndex, cactus, candidate,
                resolution);
        if (candidateState == FEATURE_BLOCK_UNKNOWN)
            return false;
        if (candidateState != FEATURE_BLOCK_AIR)
            continue;

        bool survives = true;
        static const int horizontal[4][2] = {
            {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        };
        for (const auto& offset : horizontal)
        {
            Pos3 adjacent = candidate;
            adjacent.x += offset[0];
            adjacent.z += offset[1];
            const FeatureBlockState state =
                featureBlockState(
                    layout, pieceIndex, cactus, adjacent,
                    resolution);
            if (state == FEATURE_BLOCK_UNKNOWN)
                return false;
            if (state == FEATURE_BLOCK_OCCUPIED)
            {
                resolution->fail(
                    VILLAGE_LOOT_UNRESOLVED_TEMPLATE_STATE);
                return false;
            }
            if (state == FEATURE_BLOCK_TREE_FREE_SOLID ||
                state == FEATURE_BLOCK_TREE_FREE_STURDY ||
                (state != FEATURE_BLOCK_AIR &&
                 state != FEATURE_BLOCK_TREE_FREE &&
                 state != FEATURE_BLOCK_WATER))
            {
                survives = false;
                break;
            }
        }
        if (!survives)
            continue;

        Pos3 below = candidate;
        below.y--;
        const FeatureBlockState belowState =
            featureBlockState(
                layout, pieceIndex, cactus, below,
                resolution);
        if (belowState == FEATURE_BLOCK_UNKNOWN)
            return false;
        if (belowState != FEATURE_BLOCK_SAND &&
            belowState != FEATURE_BLOCK_PLACED)
        {
            continue;
        }

        Pos3 above = candidate;
        above.y++;
        const FeatureBlockState aboveState =
            featureBlockState(
                layout, pieceIndex, cactus, above,
                resolution);
        if (aboveState == FEATURE_BLOCK_UNKNOWN ||
            aboveState == FEATURE_BLOCK_OCCUPIED)
        {
            if (aboveState == FEATURE_BLOCK_OCCUPIED)
            {
                resolution->fail(
                    VILLAGE_LOOT_UNRESOLVED_TEMPLATE_STATE);
            }
            return false;
        }
        if (aboveState == FEATURE_BLOCK_WATER)
            continue;

        const int extra = nextInt(&advanced, 3);
        const int height =
            1 + nextInt(&advanced, extra + 1);
        for (int y = 0; y < height; y++)
        {
            Pos3 placed = candidate;
            placed.y += y;
            cactus.push_back(placed);
        }
    }
    *random = advanced;
    return true;
}

enum VillageTreeType
{
    TREE_NORMAL,
    TREE_PINE,
    TREE_SPRUCE,
    TREE_ACACIA,
};

bool treeAreaIsFree(
    const VillageLayout16& layout, int pieceIndex,
    int baseX, int baseY, int baseZ, int treeHeight,
    int limit, int lowerRadius, int upperRadius,
    bool *treeFits, FeatureResolution16 *resolution)
{
    *treeFits = true;
    const QVector<Pos3> noPlacedBlocks;
    for (int y = 0; y <= treeHeight + 1; y++)
    {
        const int radius =
            y < limit ? lowerRadius : upperRadius;
        for (int z = -radius; z <= radius; z++)
        {
            for (int x = -radius; x <= radius; x++)
            {
                const FeatureBlockState state =
                    featureBlockState(
                        layout, pieceIndex, noPlacedBlocks,
                        Pos3{
                            baseX + x,
                            baseY + y,
                            baseZ + z,
                        }, resolution);
                if (state == FEATURE_BLOCK_UNKNOWN)
                    return false;
                if (state != FEATURE_BLOCK_AIR &&
                    state != FEATURE_BLOCK_TREE_FREE &&
                    state != FEATURE_BLOCK_TREE_FREE_SOLID &&
                    state != FEATURE_BLOCK_TREE_FREE_STURDY &&
                    state != FEATURE_BLOCK_WATER)
                {
                    *treeFits = false;
                    return true;
                }
            }
        }
    }
    return true;
}

bool placeAcaciaLog(
    const VillageLayout16& layout, int pieceIndex,
    QVector<Pos3> *logs, const Pos3& pos,
    bool *placed, FeatureResolution16 *resolution)
{
    const FeatureBlockState state = featureBlockState(
        layout, pieceIndex, *logs, pos, resolution);
    if (state == FEATURE_BLOCK_UNKNOWN)
        return false;
    *placed = state == FEATURE_BLOCK_AIR ||
        state == FEATURE_BLOCK_TREE_FREE ||
        state == FEATURE_BLOCK_TREE_FREE_SOLID ||
        state == FEATURE_BLOCK_TREE_FREE_STURDY ||
        state == FEATURE_BLOCK_WATER;
    if (*placed)
        logs->push_back(pos);
    return true;
}

bool stateRaisesOceanFloor(FeatureBlockState state)
{
    return state == FEATURE_BLOCK_PATH ||
        state == FEATURE_BLOCK_STURDY ||
        state == FEATURE_BLOCK_SAND ||
        state == FEATURE_BLOCK_SOIL ||
        state == FEATURE_BLOCK_TREE_FREE_SOLID ||
        state == FEATURE_BLOCK_TREE_FREE_STURDY;
}

bool treeRuntimeBase(
    const VillageLayout16& layout, int pieceIndex,
    const Pos3& origin, int *baseY, bool *mayGrow,
    FeatureResolution16 *resolution)
{
    const auto surface = layout.featureSurfaceHeights.constFind(
        horizontalKey(origin.x, origin.z));
    if (surface == layout.featureSurfaceHeights.constEnd())
    {
        resolution->fail(VILLAGE_LOOT_UNRESOLVED_SURFACE_MISSING);
        return false;
    }

    int worldSurface = *surface;
    int oceanFloor = *surface > 63 ? *surface : -1;
    QVector<int> candidateY;
    for (auto blocks = layout.placedBlocks.constBegin();
         blocks != layout.placedBlocks.constEnd(); ++blocks)
    {
        for (const VillagePlacedBlock16& block : *blocks)
        {
            if (block.pieceIndex < pieceIndex &&
                block.pos.x == origin.x &&
                block.pos.z == origin.z)
            {
                candidateY.push_back(block.pos.y);
            }
        }
    }
    for (const VillagePathBlock16& path : layout.grassPaths)
    {
        if (path.pieceIndex < pieceIndex &&
            path.pos.x == origin.x &&
            path.pos.z == origin.z)
        {
            candidateY.push_back(path.pos.y);
        }
    }
    std::sort(candidateY.begin(), candidateY.end());
    candidateY.erase(
        std::unique(candidateY.begin(), candidateY.end()),
        candidateY.end());

    const QVector<Pos3> noFeatureBlocks;
    for (int y : candidateY)
    {
        const FeatureBlockState state = featureBlockState(
            layout, pieceIndex, noFeatureBlocks,
            Pos3{origin.x, y, origin.z}, resolution);
        if (state == FEATURE_BLOCK_UNKNOWN)
            return false;
        if (state == FEATURE_BLOCK_AIR)
            continue;

        worldSurface = qMax(worldSurface, y + 1);
        if (stateRaisesOceanFloor(state))
        {
            oceanFloor = qMax(oceanFloor, y + 1);
        }
        else if (state == FEATURE_BLOCK_OCCUPIED &&
                 y + 1 >= worldSurface)
        {
            // The compact manifest deliberately groups several partial
            // blocks together; their OCEAN_FLOOR predicate is not uniform.
            resolution->fail(
                VILLAGE_LOOT_UNRESOLVED_TEMPLATE_STATE);
            return false;
        }
    }

    // All four Village tree configs use OCEAN_FLOOR and maxWaterDepth=0.
    // If no placed solid reaches the water surface, the precise sea floor
    // height is irrelevant: the positive water depth rejects the tree.
    if (oceanFloor < 0 || worldSurface != oceanFloor)
    {
        *mayGrow = false;
        *baseY = 0;
        return true;
    }
    *mayGrow = true;
    *baseY = oceanFloor;
    return true;
}

bool advanceTree(
    uint64_t *random, const VillageLayout16& layout,
    int pieceIndex, const Pos3& origin,
    VillageTreeType type, FeatureResolution16 *resolution)
{
    uint64_t advanced = *random;
    int treeHeight;
    int foliageHeight;
    int foliageRadius;
    int limit;
    int lowerRadius = 0;
    int upperRadius;
    if (type == TREE_NORMAL)
    {
        treeHeight = 4 +
            nextInt(&advanced, 3) +
            nextInt(&advanced, 1);
        foliageHeight = 3;
        foliageRadius = 2 + nextInt(&advanced, 1);
        limit = 1;
        upperRadius = 1;
    }
    else if (type == TREE_PINE)
    {
        treeHeight = 6 +
            nextInt(&advanced, 5) +
            nextInt(&advanced, 1);
        foliageHeight = 3 + nextInt(&advanced, 2);
        const int bareTrunk = treeHeight - foliageHeight;
        foliageRadius = 1 +
            nextInt(&advanced, 1) +
            nextInt(&advanced, bareTrunk + 1);
        limit = 2;
        upperRadius = 2;
    }
    else if (type == TREE_SPRUCE)
    {
        treeHeight = 5 +
            nextInt(&advanced, 3) +
            nextInt(&advanced, 2);
        foliageHeight = qMax(
            4, treeHeight - 1 -
                nextInt(&advanced, 2));
        foliageRadius = 2 + nextInt(&advanced, 2);
        limit = 2;
        upperRadius = 2;
    }
    else
    {
        treeHeight = 5 +
            nextInt(&advanced, 3) +
            nextInt(&advanced, 3);
        foliageHeight = 0;
        foliageRadius = 2 + nextInt(&advanced, 1);
        limit = 1;
        upperRadius = 2;
    }
    (void) foliageHeight;
    (void) foliageRadius;

    int baseY = 0;
    bool mayGrow = false;
    if (!treeRuntimeBase(
            layout, pieceIndex, origin, &baseY, &mayGrow,
            resolution))
    {
        return false;
    }
    if (!mayGrow)
    {
        *random = advanced;
        return true;
    }
    if (baseY < 1 || baseY + treeHeight + 1 > 256)
    {
        *random = advanced;
        return true;
    }
    if (layout.villageType == VillageLayout16::DESERT)
    {
        *random = advanced;
        return true;
    }
    const QVector<Pos3> noPlacedBlocks;
    const FeatureBlockState substrate = featureBlockState(
        layout, pieceIndex, noPlacedBlocks,
        Pos3{origin.x, baseY - 1, origin.z}, resolution);
    if (substrate == FEATURE_BLOCK_UNKNOWN)
        return false;
    if (substrate != FEATURE_BLOCK_SOIL)
    {
        *random = advanced;
        return true;
    }

    bool fits;
    if (!treeAreaIsFree(
            layout, pieceIndex,
            origin.x, baseY, origin.z, treeHeight,
            limit, lowerRadius, upperRadius, &fits,
            resolution))
    {
        return false;
    }
    if (!fits)
    {
        *random = advanced;
        return true;
    }

    if (type == TREE_NORMAL)
    {
        // FoliagePlacer.offset nextInt(1), followed by four Blob rows
        // whose four corners each call nextInt(2).
        (void) nextInt(&advanced, 1);
        for (int corner = 0; corner < 16; corner++)
            (void) nextInt(&advanced, 2);
    }
    else if (type == TREE_PINE)
    {
        (void) nextInt(&advanced, 1);
    }
    else if (type == TREE_SPRUCE)
    {
        (void) nextInt(&advanced, 3);
        (void) nextInt(&advanced, 2);
    }
    else
    {
        static const int directionX[4] = {0, 1, 0, -1};
        static const int directionZ[4] = {-1, 0, 1, 0};
        QVector<Pos3> logs;
        const int direction = nextInt(&advanced, 4);
        const int bendStart =
            treeHeight - nextInt(&advanced, 4) - 1;
        int bendLength = 3 - nextInt(&advanced, 3);
        int x = origin.x;
        int z = origin.z;
        for (int y = 0; y < treeHeight; y++)
        {
            if (y >= bendStart && bendLength > 0)
            {
                x += directionX[direction];
                z += directionZ[direction];
                bendLength--;
            }
            bool placed;
            if (!placeAcaciaLog(
                    layout, pieceIndex, &logs,
                    Pos3{x, baseY + y, z}, &placed,
                    resolution))
            {
                return false;
            }
        }

        int attachments = 1;
        x = origin.x;
        z = origin.z;
        const int secondDirection =
            nextInt(&advanced, 4);
        if (secondDirection != direction)
        {
            const int branchStart =
                bendStart - nextInt(&advanced, 2) - 1;
            int branchLength =
                1 + nextInt(&advanced, 3);
            bool branchPlaced = false;
            for (int y = branchStart;
                 y < treeHeight && branchLength > 0;
                 y++, branchLength--)
            {
                if (y < 1)
                    continue;
                x += directionX[secondDirection];
                z += directionZ[secondDirection];
                bool placed;
                if (!placeAcaciaLog(
                        layout, pieceIndex, &logs,
                        Pos3{x, baseY + y, z}, &placed,
                        resolution))
                {
                    return false;
                }
                branchPlaced = branchPlaced || placed;
            }
            if (branchPlaced)
                attachments++;
        }
        for (int attachment = 0;
             attachment < attachments; attachment++)
        {
            (void) nextInt(&advanced, 1);
        }
    }

    *random = advanced;
    return true;
}

bool advanceSimpleBlockPile(
    uint64_t *random, const VillageLayout16& layout,
    int pieceIndex, const Pos3& origin,
    FeatureResolution16 *resolution)
{
    uint64_t advanced = *random;
    const int radiusX = 2 + nextInt(&advanced, 2);
    const int radiusZ = 2 + nextInt(&advanced, 2);
    // BlockPos.betweenClosed iterates X first, then Y, then Z.
    for (int z = -radiusZ; z <= radiusZ; z++)
    {
        for (int y = 0; y <= 1; y++)
        {
            for (int x = -radiusX; x <= radiusX; x++)
            {
                const float threshold =
                    nextFloat(&advanced) * 10.0f -
                    nextFloat(&advanced) * 6.0f;
                bool triesPlacement =
                    float(x * x + z * z) <= threshold;
                if (!triesPlacement)
                {
                    triesPlacement =
                        double(nextFloat(&advanced)) < 0.031;
                }
                if (!triesPlacement)
                    continue;
                const VillagePathBlock16 *path =
                    villagePathAt(
                        layout, pieceIndex,
                        Pos3{
                            origin.x + x,
                            origin.y + y - 1,
                            origin.z + z,
                        });
                if (!path || !path->aboveEmpty)
                    continue;
                if (!path->stateKnown)
                {
                    resolution->fail(
                        VILLAGE_LOOT_UNRESOLVED_PATH_STATE);
                    return false;
                }
                (void) next(&advanced, 1);
            }
        }
    }
    *random = advanced;
    return true;
}

bool advanceSafeFeature(
    uint64_t *random, const VillageLayout16& layout,
    int pieceIndex, FeatureResolution16 *resolution)
{
    const VillagePiece16& piece = layout.pieces[pieceIndex];
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
    if (feature.contains(
            QLatin1String("SNOW_PILE_CONFIG")) ||
        feature.contains(
            QLatin1String("MELON_PILE_CONFIG")))
    {
        // Their SimpleStateProvider consumes no random values. Vanilla's
        // only world-dependent RNG path is a nextBoolean when placing on a
        // grass-path block.
        return advanceSimpleBlockPile(
            random, layout, pieceIndex, piece.pos,
            resolution);
    }
    if (feature.contains(
            QLatin1String("HAY_PILE_CONFIG")))
    {
        return advanceBlockPile(
            random, layout, pieceIndex, piece.pos,
            PILE_ROTATED, resolution);
    }
    if (feature.contains(
            QLatin1String("PUMPKIN_PILE_CONFIG")) ||
        feature.contains(
            QLatin1String("ICE_PILE_CONFIG")))
    {
        return advanceBlockPile(
            random, layout, pieceIndex, piece.pos,
            PILE_WEIGHTED, resolution);
    }
    if (feature.contains(
            QLatin1String("CACTUS_CONFIG")))
    {
        return advanceCactusPatch(
            random, layout, pieceIndex, piece.pos,
            resolution);
    }
    if (feature.contains(
            QLatin1String("NORMAL_TREE_CONFIG")))
    {
        return advanceTree(
            random, layout, pieceIndex, piece.pos,
            TREE_NORMAL, resolution);
    }
    if (feature.contains(
            QLatin1String("PINE_TREE_CONFIG")))
    {
        return advanceTree(
            random, layout, pieceIndex, piece.pos,
            TREE_PINE, resolution);
    }
    if (feature.contains(
            QLatin1String("SPRUCE_TREE_CONFIG")))
    {
        return advanceTree(
            random, layout, pieceIndex, piece.pos,
            TREE_SPRUCE, resolution);
    }
    if (feature.contains(
            QLatin1String("ACACIA_TREE_CONFIG")))
    {
        return advanceTree(
            random, layout, pieceIndex, piece.pos,
            TREE_ACACIA, resolution);
    }
    resolution->fail(VILLAGE_LOOT_UNRESOLVED_UNKNOWN_FEATURE);
    return false;
}

}

const char *villageLootUnresolvedReasonName16(int reason)
{
    switch (reason)
    {
    case VILLAGE_LOOT_UNRESOLVED_NONE:
        return "NONE";
    case VILLAGE_LOOT_UNRESOLVED_CROSS_CHUNK:
        return "CROSS_CHUNK";
    case VILLAGE_LOOT_UNRESOLVED_PATH_STATE:
        return "PATH_STATE";
    case VILLAGE_LOOT_UNRESOLVED_TEMPLATE_STATE:
        return "TEMPLATE_STATE";
    case VILLAGE_LOOT_UNRESOLVED_SURFACE_MISSING:
        return "SURFACE_MISSING";
    case VILLAGE_LOOT_UNRESOLVED_WATER_LEVEL:
        return "WATER_LEVEL";
    case VILLAGE_LOOT_UNRESOLVED_DEEP_TERRAIN:
        return "DEEP_TERRAIN";
    case VILLAGE_LOOT_UNRESOLVED_UNKNOWN_FEATURE:
        return "UNKNOWN_FEATURE";
    default:
        return "INVALID";
    }
}

bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChestSeed16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    const QVector<Pos>& overlappingChestChunks,
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
    QHash<qint64, bool> overlappingChunks;
    for (const Pos& chunk : overlappingChestChunks)
        overlappingChunks.insert(chunkKey(chunk.x, chunk.z), true);
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
        const int outputIndex = out->size();
        out->push_back(generated);
        outputByContainer[containerIndex] = outputIndex;
        targetChunks[chunkKey(
            blockChunk(container.pos.x),
            blockChunk(container.pos.z))].push_back(
                outputIndex);
    }

    QVector<bool> assigned(out->size(), false);
    for (auto target = targetChunks.constBegin();
         target != targetChunks.constEnd(); ++target)
    {
        const int chunkX = chunkXFromKey(target.key());
        const int chunkZ = chunkZFromKey(target.key());
        if (overlappingChunks.contains(target.key()))
        {
            for (int outputIndex : target.value())
            {
                (*out)[outputIndex].quality =
                    VILLAGE_LOOT_SEED_UNRESOLVED_OVERLAP;
                assigned[outputIndex] = true;
            }
            continue;
        }
        uint64_t random;
        setSeed(
            &random,
            getPopulationSeed(
                MC_1_16_1, worldSeed,
                chunkX * 16, chunkZ * 16) +
                UINT64_C(40011));

        bool unresolved = false;
        int unresolvedFeatureIndex = -1;
        int unresolvedReason = VILLAGE_LOOT_UNRESOLVED_NONE;
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
                    ![&] {
                        FeatureResolution16 resolution;
                        const bool exact = advanceSafeFeature(
                            &random, layout, pieceIndex,
                            &resolution);
                        if (!exact)
                        {
                            unresolvedFeatureIndex = pieceIndex;
                            unresolvedReason = resolution.reason;
                        }
                        return exact;
                    }())
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
                        (*out)[outputIndex].unresolvedFeatureIndex =
                            unresolvedFeatureIndex;
                        (*out)[outputIndex].unresolvedReason =
                            unresolvedReason;
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

bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChestSeed16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    bool anotherVillageMayReferenceAChestChunk,
    QString *error)
{
    QVector<Pos> overlappingChestChunks;
    if (anotherVillageMayReferenceAChestChunk)
    {
        QHash<qint64, bool> seen;
        for (const VillageContainer16& container :
             layout.containers)
        {
            if (container.lootTable.isEmpty())
                continue;
            const int chunkX = blockChunk(container.pos.x);
            const int chunkZ = blockChunk(container.pos.z);
            const qint64 key = chunkKey(chunkX, chunkZ);
            if (seen.contains(key))
                continue;
            seen.insert(key, true);
            overlappingChestChunks.push_back(
                Pos{chunkX, chunkZ});
        }
    }
    return assignVillageLootSeedsSingleStart16(
        out, layout, worldSeed, overlappingChestChunks,
        error);
}
