#include "cubiomes/loot.h"
#include "cubiomes/rng.h"
#include "cubiomes/finders.h"
#include <inttypes.h>
#include <stdio.h>

int getStructureConfig_override(int type, int mc, StructureConfig *config)
{
    return getStructureConfig(type, mc, config);
}

/* Compare output with LootRngOracle262 using the pinned official JAR. */
int main(void)
{
    const uint64_t seeds[] = {0, 1, UINT64_C(281474976710657), UINT64_MAX};
    const int chunks[][2] = {{0, 0}, {-7, 19}, {12, -34}};
    for (int s = 0; s < 4; ++s) for (int c = 0; c < 3; ++c)
    {
        uint64_t population = structureLootDecorationSeed26(seeds[s],
            chunks[c][0] * 16, chunks[c][1] * 16);
        Xoroshiro xr;
        xSetSeed(&xr, population + 40000);
        uint64_t first = xNextLongJ(&xr), second = xNextLongJ(&xr);
        printf("%016" PRIx64 " %d %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n",
            seeds[s], chunks[c][0], chunks[c][1], population, first, second);
    }
    return 0;
}
