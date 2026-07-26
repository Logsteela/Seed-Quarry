#include "cubiomes/loot.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: loot_table_probe <table-id> <LootTableSeed>\n");
        return 2;
    }
    int table = atoi(argv[1]);
    uint64_t seed = strtoull(argv[2], 0, 10);
    StructureLoot loot;
    if (!generateStructureLootTable16(&loot, table, seed))
        return 1;

    for (int item = 0; item < DP_LOOT_ITEM_COUNT; item++)
    {
        if (loot.count[item])
            printf("%s=%u\n", structureLootItemName(item), loot.count[item]);
    }
    for (int enchantment = 0;
         enchantment < DP_ENCH_COUNT; enchantment++)
    {
        for (int level = 1; level <= DP_ENCH_MAX_LEVEL; level++)
        {
            if (loot.enchantedBook[enchantment][level])
            {
                printf(
                    "enchanted_book@%s:%d=%u\n",
                    desertPyramidEnchantmentName(enchantment),
                    level,
                    loot.enchantedBook[enchantment][level]);
            }
        }
    }
    return 0;
}
