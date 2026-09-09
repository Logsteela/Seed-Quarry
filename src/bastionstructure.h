#ifndef BASTIONSTRUCTURE_H
#define BASTIONSTRUCTURE_H

#include "cubiomes/finders.h"

#include <QString>
#include <QVector>

#include <stdint.h>

/**
 * A randomizable container placed by a Java 1.16.1 Bastion Remnant piece.
 *
 * lootTableSeed is the value written to the chest's LootTableSeed NBT.  The
 * table value is one of StructureLootTable16 from cubiomes/loot.h.
 */
struct BastionLootChest16
{
    Pos3 pos = {};
    int table = -1;
    uint64_t lootTableSeed = 0;
    QString piece;
};

struct BastionPiece16
{
    QString name;
    Pos3 pos = {};
    Pos3 bb0 = {};
    Pos3 bb1 = {};
    int rotation = 0;
    int depth = 0;
};

struct BastionLayout16
{
    enum StartType {
        HOUSING_UNITS,
        HOGLIN_STABLE,
        TREASURE_ROOM,
        BRIDGE,
    };

    int startType = -1;
    int rotation = 0;
    int pieceCount = 0;
    QVector<BastionPiece16> pieces;
    QVector<BastionLootChest16> chests;
};

/**
 * Returns whether the generated 1.16.1 jigsaw manifest can be loaded.
 *
 * The manifest is intentionally kept outside the executable because it is
 * generated locally from the user's official Minecraft client jar.
 */
bool isBastionStructureData16Available(QString *error = nullptr);
QString bastionStructureData16Path();

/**
 * Reconstruct a Java 1.16.1 Bastion Remnant and its randomizable containers.
 *
 * chunkX/chunkZ are the start chunk, not a region coordinate.  Layout and
 * chest seeds depend only on the lower 48 bits of worldSeed in Java 1.16.1.
 */
bool generateBastionLayout16(
    BastionLayout16 *out, uint64_t worldSeed,
    int chunkX, int chunkZ, QString *error = nullptr);

/**
 * Loot-search path that omits the exported piece list. Jigsaw placement is
 * still reconstructed exactly; only metadata unused by Loot conditions is
 * skipped.
 */
bool generateBastionLootChests16(
    QVector<BastionLootChest16> *out, uint64_t worldSeed,
    int chunkX, int chunkZ, QString *error = nullptr);

#endif
