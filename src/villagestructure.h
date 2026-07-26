#ifndef VILLAGESTRUCTURE_H
#define VILLAGESTRUCTURE_H

#include "cubiomes/finders.h"

#include <QString>
#include <QVector>

#include <stdint.h>

/**
 * WORLD_SURFACE_WG equivalent used while assembling a Java 1.16.1 village.
 *
 * The return value is ChunkGenerator::getFirstFreeHeight(), i.e. the first
 * free Y above the generated base terrain column. A negative value aborts
 * generation. Implementations must be pure for a given X/Z pair.
 */
typedef int (*VillageHeightCallback16)(
    void *context, int blockX, int blockZ);

struct VillagePiece16
{
    enum ElementType {
        LEGACY_TEMPLATE,
        FEATURE,
    };

    QString name;
    QString feature;
    Pos3 pos = {};
    Pos3 bb0 = {};
    Pos3 bb1 = {};
    int rotation = 0;
    int depth = 0;
    int groundLevelDelta = 1;
    int elementType = LEGACY_TEMPLATE;
    bool terrainMatching = false;
};

struct VillageContainer16
{
    Pos3 pos = {};
    QString block;
    QString lootTable;
    int table = -1;
    int pieceIndex = -1;
    int placementIndex = -1;
    QString piece;

    bool hasLootTable() const
    {
        return table >= 0 && !lootTable.isEmpty();
    }
};

struct VillageLayout16
{
    enum VillageType {
        PLAINS,
        DESERT,
        SAVANNA,
        SNOWY,
        TAIGA,
    };

    int villageType = -1;
    int biome = -1;
    int rotation = 0;
    int pieceCount = 0;
    QString startPool;
    QVector<VillagePiece16> pieces;
    QVector<VillageContainer16> containers;
};

bool isVillageStructureData16Available(QString *error = nullptr);
QString villageStructureData16Path();

/**
 * Reconstruct a Java 1.16.1 village using an injected exact height provider.
 *
 * biomeId must be one of cubiomes' five viable 1.16.1 Village biomes:
 * plains, desert, savanna, snowy_tundra, or taiga.
 */
bool generateVillageLayout16WithHeights(
    VillageLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, int biomeId,
    VillageHeightCallback16 heightCallback, void *heightContext,
    QString *error = nullptr);

/**
 * Reconstruct a Java 1.16.1 village using cubiomes'
 * getFirstFreeHeight116() implementation.
 */
bool generateVillageLayout16(
    VillageLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, int biomeId,
    QString *error = nullptr);

#endif
