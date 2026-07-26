#include "src/bastionstructure.h"
#include "src/lootcondition.h"

#include "cubiomes/loot.h"

#include <QCoreApplication>
#include <QTextStream>

extern "C" int getStructureConfig_override(
    int structureType, int mc, StructureConfig *config)
{
    return getStructureConfig(structureType, mc, config);
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTextStream output(stdout);
    QTextStream errors(stderr);

    uint64_t seed =
        uint64_t(INT64_C(-8710769437106169188));
    int chunkX = -12;
    int chunkZ = -22;
    int argument = 1;
    bool printPieces = false;
    bool selfTest = false;
    while (argument < argc)
    {
        const QString option =
            QString::fromLocal8Bit(argv[argument]);
        if (option == QLatin1String("--pieces"))
            printPieces = true;
        else if (option == QLatin1String("--self-test"))
            selfTest = true;
        else
            break;
        argument++;
    }
    if (argc - argument == 3)
    {
        bool seedOk, xOk, zOk;
        seed = uint64_t(
            QString::fromLocal8Bit(argv[argument]).toLongLong(
                &seedOk, 10));
        chunkX = QString::fromLocal8Bit(
            argv[argument + 1]).toInt(&xOk);
        chunkZ = QString::fromLocal8Bit(
            argv[argument + 2]).toInt(&zOk);
        if (!seedOk || !xOk || !zOk)
        {
            errors << "Usage: bastion_layout_probe "
                      "[--pieces] [--self-test] "
                      "[seed chunkX chunkZ]\n";
            return 1;
        }
    }
    else if (argc != argument)
    {
        errors << "Usage: bastion_layout_probe "
                  "[--pieces] [--self-test] "
                  "[seed chunkX chunkZ]\n";
        return 1;
    }

    QString error;
    BastionLayout16 layout;
    if (!generateBastionLayout16(
            &layout, seed, chunkX, chunkZ, &error))
    {
        errors << error << '\n';
        return 2;
    }

    output << "data=" << bastionStructureData16Path() << '\n'
           << "start_type=" << layout.startType
           << " rotation=" << layout.rotation
           << " pieces=" << layout.pieceCount
           << " chests=" << layout.chests.size() << '\n';
    if (printPieces)
    {
        for (int index = 0; index < layout.pieces.size(); index++)
        {
            const BastionPiece16& piece =
                layout.pieces[index];
            output << "P|" << index << '|' << piece.name << '|'
                   << piece.pos.x << '|' << piece.pos.y << '|'
                   << piece.pos.z << '|' << piece.rotation << '|'
                   << piece.depth << '|'
                   << piece.bb0.x << '|' << piece.bb0.y << '|'
                   << piece.bb0.z << '|'
                   << piece.bb1.x << '|' << piece.bb1.y << '|'
                   << piece.bb1.z << '\n';
        }
    }
    for (int index = 0; index < layout.chests.size(); index++)
    {
        const BastionLootChest16& chest = layout.chests[index];
        output << index << ": "
               << chest.pos.x << ',' << chest.pos.y << ','
               << chest.pos.z << ' '
               << structureLootTable16Name(chest.table) << ' '
               << qint64(chest.lootTableSeed) << ' '
               << chest.piece << '\n';
        StructureLoot loot = {};
        if (!generateStructureLootTable16(
                &loot, chest.table, chest.lootTableSeed))
            return 3;
        for (int item = 0; item < DP_LOOT_ITEM_COUNT; item++)
        {
            if (loot.count[item])
            {
                output << "L|" << index << '|'
                       << structureLootItemName(item) << '|'
                       << loot.count[item] << '\n';
            }
        }
    }
    if (selfTest)
    {
        if (layout.chests.isEmpty())
        {
            errors << "Self-test needs a Bastion with a chest.\n";
            return 4;
        }
        const BastionLootChest16& target = layout.chests.first();
        LootRuleSet rules;
        rules.structureType = Bastion;
        rules.logic = LootRuleSet::LOGIC_ALL;
        rules.instanceMode = LootRuleSet::INSTANCE_ANY;
        rules.chestMode = LootRuleSet::CHEST_ANY;
        LootRule rule;
        rule.item = DP_LOOT_DIAMOND;
        rule.minCount = 0;
        rule.maxCount = -1;
        rules.rules.push_back(rule);
        rules.chestPositionMode =
            LootRuleSet::CHEST_POSITION_ABSOLUTE;
        rules.chestMinX = rules.chestMaxX = target.pos.x;
        rules.chestMinY = rules.chestMaxY = target.pos.y;
        rules.chestMinZ = rules.chestMaxZ = target.pos.z;
        if (!validateLootRuleSet(rules, MC_1_16_1).isEmpty())
        {
            errors << "Absolute position rules failed validation.\n";
            return 5;
        }

        const QByteArray encoded = serializeLootRuleSet(rules);
        LootRuleSet decoded;
        if (encoded.mid(4, 2) != QByteArray::fromHex("0200") ||
            !deserializeLootRuleSet(encoded, &decoded) ||
            decoded.chestPositionMode !=
                LootRuleSet::CHEST_POSITION_ABSOLUTE ||
            decoded.chestMinX != target.pos.x ||
            decoded.chestMinY != target.pos.y ||
            decoded.chestMinZ != target.pos.z)
        {
            errors << "Position rule serialization failed.\n";
            return 6;
        }

        const Pos structurePos = {
            chunkX * 16, chunkZ * 16,
        };
        if (!matchStructureLoot(
                rules, MC_1_16_1, seed, structurePos))
        {
            errors << "Known absolute chest position did not match.\n";
            return 7;
        }
        LootRuleSet missing = rules;
        missing.chestMinX = missing.chestMaxX = 30000000;
        missing.chestMinY = missing.chestMaxY = 2048;
        missing.chestMinZ = missing.chestMaxZ = 30000000;
        if (matchStructureLoot(
                missing, MC_1_16_1, seed, structurePos))
        {
            errors << "Missing absolute chest position matched.\n";
            return 8;
        }

        LootRuleSet relative = rules;
        relative.chestPositionMode =
            LootRuleSet::CHEST_POSITION_RELATIVE;
        relative.chestMinX = relative.chestMaxX =
            target.pos.x - chunkX * 16;
        relative.chestMinY = relative.chestMaxY =
            target.pos.y - 32;
        relative.chestMinZ = relative.chestMaxZ =
            target.pos.z - chunkZ * 16;
        if (!matchStructureLoot(
                relative, MC_1_16_1, seed, structurePos))
        {
            errors << "Known relative chest position did not match.\n";
            return 9;
        }

        LootRuleSet legacy = rules;
        legacy.chestPositionMode =
            LootRuleSet::CHEST_POSITION_ANY;
        if (serializeLootRuleSet(legacy).mid(4, 2) !=
            QByteArray::fromHex("0100"))
        {
            errors << "Legacy Loot rule encoding changed.\n";
            return 10;
        }
        const uint64_t sameLower48 =
            (seed & MASK48) | (UINT64_C(0x1234) << 48);
        if (!matchStructureLoot(
                rules, MC_1_16_1, sameLower48, structurePos))
        {
            errors << "Upper 16 bits changed Bastion chest matching.\n";
            return 11;
        }
        output << "self_test=ok\n";
    }
    return 0;
}
