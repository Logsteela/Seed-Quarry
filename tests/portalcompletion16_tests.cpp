#include "src/portalcompletion16.h"

#include "cubiomes/biomenoise.h"
#include "cubiomes/generator.h"

#include <cassert>
#include <cstdio>

extern "C" int getStructureConfig_override(
    int structureType, int mc, StructureConfig *config)
{
    return getStructureConfig(structureType, mc, config);
}

int main()
{
    struct Fixture {
        uint64_t seed;
        int mc, chunkX, chunkZ;
        int portal, portalY;
        bool loot, frame, complete;
    };
    const Fixture fixtures[] = {
        // SeedFinding reference chest (113,38,1629): portal_9's chest has
        // local Y=1, so the template anchor is Y=37.
        {UINT64_C(7948314503011477316), MC_1_16_5,
            7, 101, 9, 37, false, true, false},
        // SeedFinding reference chest (145,69,116): portal_6's chest has
        // local Y=1, so the template anchor is Y=68. This is also a complete
        // positive fixture for the combined Loot and frame condition.
        {uint64_t(INT64_C(-7387955057302025707)), MC_1_16_1,
            9, 7, 6, 68, true, true, true},
    };
    for (const Fixture& fixture : fixtures)
    {
        Pos pos = {fixture.chunkX * 16, fixture.chunkZ * 16};
        Generator generator;
        setupGenerator(&generator, fixture.mc, 0);
        applySeed(&generator, DIM_OVERWORLD, fixture.seed);
        int biome = getBiomeAt(
            &generator, 4, (pos.x >> 2) + 2, 0, (pos.z >> 2) + 2);
        assert(biome >= 0);

        StructureVariant variant = {};
        assert(getVariant(
            &variant, Ruined_Portal, fixture.mc,
            fixture.seed, pos.x, pos.z, biome));

        SurfaceNoise surfaceNoise;
        initSurfaceNoise(&surfaceNoise, DIM_OVERWORLD, fixture.seed);
        RuinedPortalCompletion16Details details;
        bool complete = isSelfCompletableRuinedPortal16(
            fixture.seed, fixture.mc, pos, variant,
            &generator, &surfaceNoise, &details);

        std::printf(
            "seed=%llu chunk=%d,%d biome=%d category=%d "
            "portal_%d rotation=%d mirrorRoll=%d y=%d loot=%d "
            "frame=%d complete=%d\n",
            (unsigned long long) fixture.seed,
            fixture.chunkX, fixture.chunkZ, biome, variant.biome,
            variant.start, variant.rotation,
            variant.mirror, details.portalY, details.lootSufficient,
            details.frameSufficient, complete);

        assert(details.supported);
        assert(variant.start == fixture.portal);
        assert(details.portalY == fixture.portalY);
        assert(details.lootSufficient == fixture.loot);
        assert(details.frameSufficient == fixture.frame);
        assert(complete == fixture.complete);
    }

    // The 26.2 path must use all 64 bits for Loot/terrain while preserving
    // the lower-48-dependent portal template transform. Find a bounded local
    // fixture rather than baking an approximate terrain height into the test.
    bool checkedModern = false;
    bool observedUpperDifference = false;
    for (uint64_t low = 0; low < 256 && !observedUpperDifference; low++)
    {
        Pos pos = {0, 0};
        Generator lowerGenerator, upperGenerator;
        setupGenerator(&lowerGenerator, MC_26_2, 0);
        setupGenerator(&upperGenerator, MC_26_2, 0);
        applySeed(&lowerGenerator, DIM_OVERWORLD, low);
        applySeed(&upperGenerator, DIM_OVERWORLD,
            low + (UINT64_C(1) << 48));
        int lowerBiome = getBiomeAt(&lowerGenerator, 4, 2, 0, 2);
        int upperBiome = getBiomeAt(&upperGenerator, 4, 2, 0, 2);
        StructureVariant lowerVariant = {}, upperVariant = {};
        assert(getVariant(&lowerVariant, Ruined_Portal, MC_26_2,
            low, 0, 0, lowerBiome));
        assert(getVariant(&upperVariant, Ruined_Portal, MC_26_2,
            low + (UINT64_C(1) << 48), 0, 0, upperBiome));
        if (lowerVariant.giant || (lowerVariant.start != 1 &&
                lowerVariant.start != 6 && lowerVariant.start != 9) ||
            upperVariant.biome != lowerVariant.biome)
            continue;
        RuinedPortalCompletion16Details lowerDetails, upperDetails;
        bool lower = isSelfCompletableRuinedPortal26(low, pos,
            lowerVariant, &lowerGenerator, &lowerDetails);
        bool upper = isSelfCompletableRuinedPortal26(
            low + (UINT64_C(1) << 48), pos,
            upperVariant, &upperGenerator, &upperDetails);
        assert(lowerDetails.supported && upperDetails.supported);
        assert(lowerDetails.approximateTerrain &&
            upperDetails.approximateTerrain);
        assert(lower == (lowerDetails.lootSufficient &&
            lowerDetails.frameSufficient));
        assert(upper == (upperDetails.lootSufficient &&
            upperDetails.frameSufficient));
        checkedModern = true;
        observedUpperDifference =
            lowerDetails.portalY != upperDetails.portalY ||
            lowerDetails.lootSufficient != upperDetails.lootSufficient ||
            lowerDetails.frameSufficient != upperDetails.frameSufficient;
    }
    assert(checkedModern && observedUpperDifference);
    return 0;
}
