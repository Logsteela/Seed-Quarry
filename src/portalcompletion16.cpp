#include "portalcompletion16.h"

#include "cubiomes/loot.h"
#include "cubiomes/rng.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

struct Point3
{
    int x, y, z;
};

struct PortalTemplate
{
    int index;
    Point3 size;
    const Point3 *minimalFrame;
    int minimalFrameCount;
    int requiredObsidian;
};

static const Point3 portal1Frame[] = {
    {3,2,2}, {3,2,3}, {3,3,1}, {3,3,4},
    {3,4,1}, {3,5,1}, {3,6,2}, {3,6,3},
};
static const Point3 portal6Frame[] = {
    {2,1,1}, {2,1,2}, {2,1,3}, {2,2,0}, {2,2,4},
    {2,3,0}, {2,3,4}, {2,4,0}, {2,4,4}, {2,5,1},
    {2,5,3},
};
static const Point3 portal9Frame[] = {
    {4,1,4}, {4,1,5}, {4,2,3}, {4,2,6},
    {4,3,6}, {4,4,6}, {4,5,4}, {4,5,5},
};

static const PortalTemplate portalTemplates[] = {
    {1, {6,10,6}, portal1Frame, int(sizeof(portal1Frame) / sizeof(*portal1Frame)), 2},
    {6, {5,7,7},  portal6Frame, int(sizeof(portal6Frame) / sizeof(*portal6Frame)), 1},
    {9, {10,8,9}, portal9Frame, int(sizeof(portal9Frame) / sizeof(*portal9Frame)), 2},
};

static const PortalTemplate *getPortalTemplate(const StructureVariant& variant)
{
    if (variant.giant)
        return nullptr;
    for (const PortalTemplate& portal : portalTemplates)
        if (portal.index == variant.start)
            return &portal;
    return nullptr;
}

static int floorDiv4(int value)
{
    int quotient = value / 4;
    if (value % 4 < 0)
        quotient--;
    return quotient;
}

static Point3 transformPoint(
    Point3 point, const Point3& pivot, int rotation, bool frontBackMirror)
{
    if (frontBackMirror)
        point.x = -point.x;

    const int x = point.x;
    const int z = point.z;
    switch (rotation)
    {
    case 1:
        point.x = pivot.x + pivot.z - z;
        point.z = pivot.z - pivot.x + x;
        break;
    case 2:
        point.x = 2 * pivot.x - x;
        point.z = 2 * pivot.z - z;
        break;
    case 3:
        point.x = pivot.x - pivot.z + z;
        point.z = pivot.x + pivot.z - x;
        break;
    default:
        break;
    }
    return point;
}

struct PortalBox
{
    int minX, minZ, maxX, maxZ;
};

static PortalBox getPortalBox(
    Pos anchor, const PortalTemplate& portal, int rotation,
    bool frontBackMirror)
{
    const Point3 pivot = {portal.size.x / 2, 0, portal.size.z / 2};
    PortalBox box = {
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::min(),
    };
    for (int x : {0, portal.size.x - 1})
    {
        for (int z : {0, portal.size.z - 1})
        {
            Point3 p = transformPoint({x, 0, z}, pivot, rotation,
                frontBackMirror);
            p.x += anchor.x;
            p.z += anchor.z;
            box.minX = std::min(box.minX, p.x);
            box.maxX = std::max(box.maxX, p.x);
            box.minZ = std::min(box.minZ, p.z);
            box.maxZ = std::max(box.maxZ, p.z);
        }
    }
    return box;
}

enum PortalLocation
{
    PORTAL_ON_LAND,
    PORTAL_ON_OCEAN_FLOOR,
    PORTAL_UNDERGROUND,
    PORTAL_IN_MOUNTAIN,
    PORTAL_PARTLY_BURIED,
};

static PortalLocation getPortalLocation(const StructureVariant& variant)
{
    if (variant.biome == desert)
        return PORTAL_PARTLY_BURIED;
    if (variant.biome == swamp || variant.biome == ocean)
        return PORTAL_ON_OCEAN_FLOOR;
    if (variant.biome == mountains)
        return variant.underground ? PORTAL_IN_MOUNTAIN : PORTAL_ON_LAND;
    if (variant.biome == plains)
        return variant.underground ? PORTAL_UNDERGROUND : PORTAL_ON_LAND;
    return PORTAL_ON_LAND; // jungle
}

static int nextIntInclusive(uint64_t *random, int minimum, int maximum)
{
    if (minimum >= maximum)
        return minimum;
    return minimum + nextInt(random, maximum - minimum + 1);
}

static bool advancePortalVariantRandom(
    uint64_t worldSeed, Pos pos, const StructureVariant& variant,
    uint64_t *random, bool *frontBackMirror)
{
    *random = chunkGenerateRnd(worldSeed, pos.x >> 4, pos.z >> 4);

    if (variant.biome == plains || variant.biome == mountains)
    {
        bool inside = nextFloat(random) < 0.5f;
        if (!inside)
            (void) nextFloat(random);
        if (inside != bool(variant.underground))
            return false;
    }
    else if (variant.biome == jungle)
    {
        (void) nextFloat(random);
    }

    bool giant = nextFloat(random) < 0.05f;
    int start = 1 + nextInt(random, giant ? 3 : 10);
    int rotation = nextInt(random, 4);
    bool noMirror = nextFloat(random) < 0.5f;
    if (giant != bool(variant.giant) || start != variant.start ||
        rotation != variant.rotation || noMirror != bool(variant.mirror))
        return false;

    // Vanilla uses NONE for a roll below 0.5 and FRONT_BACK otherwise.
    // StructureVariant::mirror stores that raw below-0.5 roll.
    *frontBackMirror = !noMirror;
    return true;
}

class TerrainColumns116
{
public:
    TerrainColumns116(
        const Generator *generator, const SurfaceNoise *surfaceNoise,
        const std::array<Pos, 5>& points)
        : m_minX(std::numeric_limits<int>::max())
        , m_minZ(std::numeric_limits<int>::max())
        , m_width(0)
        , m_height(0)
        , m_ok(false)
    {
        int maxX = std::numeric_limits<int>::min();
        int maxZ = std::numeric_limits<int>::min();
        for (Pos point : points)
        {
            int cellX = floorDiv4(point.x);
            int cellZ = floorDiv4(point.z);
            m_minX = std::min(m_minX, cellX);
            m_minZ = std::min(m_minZ, cellZ);
            maxX = std::max(maxX, cellX + 1);
            maxZ = std::max(maxZ, cellZ + 1);
        }
        m_width = maxX - m_minX + 1;
        m_height = maxZ - m_minZ + 1;
        if (m_width <= 0 || m_height <= 0 ||
            m_width * m_height > int(m_columns.size()))
            return;
        m_ok = getTerrainNoiseColumns116(
            generator, surfaceNoise, m_minX, m_minZ,
            m_width, m_height, m_columns[0].data()) != 0;
    }

    bool ok() const { return m_ok; }

    bool matchesHeightmap(Pos point, int y, bool oceanFloor) const
    {
        double density;
        if (!getDensity(point, y, &density))
            return false;
        return density > 0.0 || (!oceanFloor && y < 63);
    }

    int firstFreeHeight(Pos point, bool oceanFloor) const
    {
        for (int y = 255; y >= 0; y--)
            if (matchesHeightmap(point, y, oceanFloor))
                return y + 1;
        return 0;
    }

private:
    bool getDensity(Pos point, int y, double *density) const
    {
        if (!m_ok || !density || y < 0 || y > 255)
            return false;
        int cellX = floorDiv4(point.x);
        int cellZ = floorDiv4(point.z);
        int offsetX = point.x - cellX * 4;
        int offsetZ = point.z - cellZ * 4;
        int columnX = cellX - m_minX;
        int columnZ = cellZ - m_minZ;
        if (columnX < 0 || columnX + 1 >= m_width ||
            columnZ < 0 || columnZ + 1 >= m_height)
            return false;

        int cellY = y / 8;
        int localY = y % 8;
        const double *c00 = column(columnX, columnZ);
        const double *c01 = column(columnX, columnZ + 1);
        const double *c10 = column(columnX + 1, columnZ);
        const double *c11 = column(columnX + 1, columnZ + 1);
        *density = lerp3(
            localY / 8.0, offsetX / 4.0, offsetZ / 4.0,
            c00[cellY], c00[cellY + 1],
            c10[cellY], c10[cellY + 1],
            c01[cellY], c01[cellY + 1],
            c11[cellY], c11[cellY + 1]);
        return true;
    }

    const double *column(int x, int z) const
    {
        return m_columns[z * m_width + x].data();
    }

    int m_minX, m_minZ, m_width, m_height;
    bool m_ok;
    std::array<std::array<double, 33>, 25> m_columns = {};
};

class TerrainColumns26
{
public:
    TerrainColumns26(const Generator *generator,
        const std::array<Pos, 5>& points)
        : m_ok(generator && generator->mc == MC_26_2 &&
               generator->dim == DIM_OVERWORLD)
    {
        if (!m_ok)
            return;
        for (int i = 0; i < int(points.size()); i++)
        {
            float height = 0.0f;
            if (mapApproxHeight(&height, nullptr, generator, nullptr,
                    floorDiv4(points[i].x), floorDiv4(points[i].z), 1, 1))
            {
                m_ok = false;
                return;
            }
            m_points[i] = points[i];
            m_heights[i] = int(std::floor(height));
        }
    }

    bool ok() const { return m_ok; }

    int firstFreeHeight(Pos point, bool) const
    {
        for (int i = 0; i < int(m_points.size()); i++)
            if (m_points[i].x == point.x && m_points[i].z == point.z)
                return m_heights[i] + 1;
        return -63;
    }

    bool matchesHeightmap(Pos point, int y, bool) const
    {
        return y < firstFreeHeight(point, false);
    }

private:
    bool m_ok;
    std::array<Pos, 5> m_points = {};
    std::array<int, 5> m_heights = {};
};

static int findPortalY(
    const PortalTemplate& portal, const PortalBox& box,
    PortalLocation location, uint64_t *random,
    const TerrainColumns116& terrain)
{
    bool oceanFloor = location == PORTAL_ON_OCEAN_FLOOR;
    Pos center = {
        box.minX + (box.maxX - box.minX + 1) / 2,
        box.minZ + (box.maxZ - box.minZ + 1) / 2,
    };
    int height = terrain.firstFreeHeight(center, oceanFloor) - 1;
    int y;
    if (location == PORTAL_IN_MOUNTAIN)
        y = nextIntInclusive(random, 70, height - portal.size.y);
    else if (location == PORTAL_UNDERGROUND)
        y = nextIntInclusive(random, 15, height - portal.size.y);
    else if (location == PORTAL_PARTLY_BURIED)
        y = height - portal.size.y + nextIntInclusive(random, 2, 8);
    else
        y = height;

    const Pos corners[] = {
        {box.minX, box.minZ}, {box.maxX, box.minZ},
        {box.minX, box.maxZ}, {box.maxX, box.maxZ},
    };
    for (int dig = y; dig > 15; dig--)
    {
        int matches = 0;
        for (Pos corner : corners)
        {
            if (terrain.matchesHeightmap(corner, dig, oceanFloor) &&
                ++matches == 3)
                return dig;
        }
    }
    return 15;
}

static int findPortalY26(
    const PortalTemplate& portal, const PortalBox& box,
    PortalLocation location, uint64_t *random,
    const TerrainColumns26& terrain)
{
    bool oceanFloor = location == PORTAL_ON_OCEAN_FLOOR;
    Pos center = {
        box.minX + (box.maxX - box.minX + 1) / 2,
        box.minZ + (box.maxZ - box.minZ + 1) / 2,
    };
    int height = terrain.firstFreeHeight(center, oceanFloor) - 1;
    const int minimumY = -64 + 15;
    int y;
    if (location == PORTAL_IN_MOUNTAIN)
        y = nextIntInclusive(random, 70, height - portal.size.y);
    else if (location == PORTAL_UNDERGROUND)
        y = nextIntInclusive(random, minimumY, height - portal.size.y);
    else if (location == PORTAL_PARTLY_BURIED)
        y = height - portal.size.y + nextIntInclusive(random, 2, 8);
    else
        y = height;

    const Pos corners[] = {
        {box.minX, box.minZ}, {box.maxX, box.minZ},
        {box.minX, box.maxZ}, {box.maxX, box.maxZ},
    };
    for (int dig = y; dig > minimumY; dig--)
    {
        int matches = 0;
        for (Pos corner : corners)
            if (terrain.matchesHeightmap(corner, dig, oceanFloor) &&
                ++matches == 3)
                return dig;
    }
    return minimumY;
}

static uint64_t blockPositionRandomSeed(Point3 point)
{
    // Mth.getSeed(int,int,int), including Java's overflowing int x product.
    const int32_t first = int32_t(
        uint32_t(point.x) * UINT32_C(3129871));
    uint64_t value =
        uint64_t(int64_t(first)) ^
        uint64_t(int64_t(point.z) * INT64_C(116129781)) ^
        uint64_t(int64_t(point.y));
    value = value * value * UINT64_C(42317861) + value * UINT64_C(11);
    return value >> 16;
}

static bool isCryingObsidian(Point3 point)
{
    uint64_t random;
    setSeed(&random, blockPositionRandomSeed(point));
    return nextFloat(&random) < 0.15f;
}

} // namespace

bool isSelfCompletableRuinedPortal16(
    uint64_t worldSeed,
    int mc,
    Pos structurePos,
    const StructureVariant& variant,
    const Generator *generator,
    const SurfaceNoise *surfaceNoise,
    RuinedPortalCompletion16Details *details)
{
    RuinedPortalCompletion16Details result;
    const PortalTemplate *portal = getPortalTemplate(variant);
    if ((mc != MC_1_16_1 && mc != MC_1_16_5) ||
        !generator || !surfaceNoise || !portal)
    {
        if (details)
            *details = result;
        return false;
    }
    result.supported = true;
    result.requiredObsidian = portal->requiredObsidian;

    // Loot is seed-only and much cheaper than terrain placement. Reject it
    // before generating any density columns.
    StructureLoot loot = {};
    if (!getRuinedPortalLoot16(
            &loot, worldSeed, structurePos.x >> 4, structurePos.z >> 4))
    {
        if (details)
            *details = result;
        return false;
    }
    bool ignition =
        loot.count[DP_LOOT_FLINT_AND_STEEL] > 0 ||
        loot.count[DP_LOOT_FIRE_CHARGE] > 0;
    result.lootSufficient = ignition &&
        loot.count[DP_LOOT_OBSIDIAN] >= portal->requiredObsidian;
    if (!result.lootSufficient && !details)
        return false;

    uint64_t random;
    bool frontBackMirror;
    if (!advancePortalVariantRandom(
            worldSeed, structurePos, variant, &random, &frontBackMirror))
    {
        if (details)
            *details = result;
        return false;
    }

    PortalBox box = getPortalBox(
        structurePos, *portal, variant.rotation, frontBackMirror);
    Pos center = {
        box.minX + (box.maxX - box.minX + 1) / 2,
        box.minZ + (box.maxZ - box.minZ + 1) / 2,
    };
    std::array<Pos, 5> terrainPoints = {{
        center,
        {box.minX, box.minZ}, {box.maxX, box.minZ},
        {box.minX, box.maxZ}, {box.maxX, box.maxZ},
    }};
    TerrainColumns116 terrain(generator, surfaceNoise, terrainPoints);
    if (!terrain.ok())
    {
        if (details)
            *details = result;
        return false;
    }

    int portalY = findPortalY(
        *portal, box, getPortalLocation(variant), &random, terrain);
    result.portalY = portalY;

    const Point3 pivot = {portal->size.x / 2, 0, portal->size.z / 2};
    result.frameSufficient = true;
    for (int i = 0; i < portal->minimalFrameCount; i++)
    {
        Point3 point = transformPoint(
            portal->minimalFrame[i], pivot, variant.rotation,
            frontBackMirror);
        point.x += structurePos.x;
        point.y += portalY;
        point.z += structurePos.z;
        if (isCryingObsidian(point))
        {
            result.frameSufficient = false;
            break;
        }
    }

    if (details)
        *details = result;
    return result.lootSufficient && result.frameSufficient;
}

bool isSelfCompletableRuinedPortal26(
    uint64_t worldSeed,
    Pos structurePos,
    const StructureVariant& variant,
    const Generator *generator,
    RuinedPortalCompletion16Details *details)
{
    RuinedPortalCompletion16Details result;
    const PortalTemplate *portal = getPortalTemplate(variant);
    if (!generator || generator->mc != MC_26_2 ||
        generator->dim != DIM_OVERWORLD || !portal)
    {
        if (details) *details = result;
        return false;
    }
    result.supported = true;
    result.approximateTerrain = true;
    result.requiredObsidian = portal->requiredObsidian;

    // Modern chest contents depend on the full 64-bit seed. Keep this exact,
    // cheap rejection ahead of the approximate terrain/Y calculation.
    StructureLoot loot = {};
    if (!getRuinedPortalLoot26(&loot, worldSeed,
            structurePos.x >> 4, structurePos.z >> 4,
            variant.biome, 0))
    {
        if (details) *details = result;
        return false;
    }
    const bool ignition = loot.count[DP_LOOT_FLINT_AND_STEEL] > 0 ||
        loot.count[DP_LOOT_FIRE_CHARGE] > 0;
    result.lootSufficient = ignition &&
        loot.count[DP_LOOT_OBSIDIAN] >= portal->requiredObsidian;
    if (!result.lootSufficient && !details)
        return false;

    uint64_t random;
    bool frontBackMirror;
    if (!advancePortalVariantRandom(
            worldSeed, structurePos, variant, &random, &frontBackMirror))
    {
        if (details) *details = result;
        return false;
    }

    PortalBox box = getPortalBox(structurePos, *portal,
        variant.rotation, frontBackMirror);
    Pos center = {
        box.minX + (box.maxX - box.minX + 1) / 2,
        box.minZ + (box.maxZ - box.minZ + 1) / 2,
    };
    std::array<Pos, 5> terrainPoints = {{
        center,
        {box.minX, box.minZ}, {box.maxX, box.minZ},
        {box.minX, box.maxZ}, {box.maxX, box.maxZ},
    }};
    TerrainColumns26 terrain(generator, terrainPoints);
    if (!terrain.ok())
    {
        if (details) *details = result;
        return false;
    }
    result.portalY = findPortalY26(*portal, box,
        getPortalLocation(variant), &random, terrain);

    const Point3 pivot = {portal->size.x / 2, 0, portal->size.z / 2};
    result.frameSufficient = true;
    for (int i = 0; i < portal->minimalFrameCount; i++)
    {
        Point3 point = transformPoint(portal->minimalFrame[i], pivot,
            variant.rotation, frontBackMirror);
        point.x += structurePos.x;
        point.y += result.portalY;
        point.z += structurePos.z;
        if (isCryingObsidian(point))
        {
            result.frameSufficient = false;
            break;
        }
    }
    if (details) *details = result;
    return result.lootSufficient && result.frameSufficient;
}
