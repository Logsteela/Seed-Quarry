#include "src/bastionstructure.h"
#include "cubiomes/loot.h"
#include <QCoreApplication>
#include <cstdio>

extern "C" int getStructureConfig_override(int type, int mc, StructureConfig *config)
{
    return getStructureConfig(type, mc, config);
}

// Console-only, bounded data-load/cache/enchantment smoke check.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QString error;
    if (!isBastionStructureData26Available(&error))
    {
        std::fprintf(stderr, "%s\n", error.toUtf8().constData());
        return 1;
    }
    QVector<BastionLootChest16> first, upper, repeat;
    if (!generateBastionLootChests26(&first, 1, -12, -22, &error) ||
        !generateBastionLootChests26(&upper, UINT64_C(281474976710657), -12, -22, &error) ||
        !generateBastionLootChests26(&repeat, 1, -12, -22, &error) || first.isEmpty() ||
        first.size() != upper.size() || first.size() != repeat.size()) return 2;
    for (int i = 0; i < first.size(); i++)
    {
        if (first[i].pos.x != upper[i].pos.x || first[i].pos.y != upper[i].pos.y ||
            first[i].pos.z != upper[i].pos.z || first[i].lootTableSeed == upper[i].lootTableSeed ||
            first[i].lootTableSeed != repeat[i].lootTableSeed) return 3;
        StructureLoot loot;
        if (!generateStructureLootTable16(&loot,
                LOOT_TABLE26_BASTION_BRIDGE + first[i].table - LOOT_TABLE16_BASTION_BRIDGE,
                first[i].lootTableSeed)) return 4;
    }
    if (!structureLootEnchantmentAvailable(Bastion, DP_LOOT_DIAMOND_PICKAXE, DP_ENCH_EFFICIENCY, LOOT_PROFILE_26_2) ||
        !structureLootEnchantmentAvailable(Bastion, DP_LOOT_GOLDEN_BOOTS, DP_ENCH_SOUL_SPEED, LOOT_PROFILE_26_2) ||
        !structureLootEnchantmentAvailable(Bastion, DP_LOOT_DIAMOND_SPEAR, DP_ENCH_LUNGE, LOOT_PROFILE_26_2) ||
        !structureLootEnchantmentAvailable(Ruined_Portal, DP_LOOT_GOLDEN_PICKAXE, DP_ENCH_EFFICIENCY, LOOT_PROFILE_26_2)) return 5;
    StructureLoot portal;
    if (!getRuinedPortalLoot26(&portal, 1, -12, -22, plains, 0)) return 6;
    std::printf("26.2 smoke passed: %lld Bastion chests; full-seed Loot, layout cache, portal and enchantments.\n",
        static_cast<long long>(first.size()));
    return 0;
}
