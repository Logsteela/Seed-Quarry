# Minecraft 1.16.1 Village placement RNG notes

This note is about the `WorldgenRandom` shared by Village structure pieces
during `SURFACE_STRUCTURES` placement. It is separate from the RNG used to
choose the Village jigsaw layout.

## Seed and placement order

- `ChunkGenerator.applyBiomeDecoration` starts with
  `setDecorationSeed(worldSeed, chunkBlockX, chunkBlockZ)`.
- In `Biome.generate`, Village is the 12th structure registered for
  `SURFACE_STRUCTURES` (zero-based feature index 11, decoration ordinal 4).
  Its placement RNG is therefore reset with
  `setFeatureSeed(decorationSeed, 11, 4)`, i.e.
  `decorationSeed + 40011`.
- `StructureStart.placeInChunk` traverses `pieces` in stored order. Only pieces
  whose stored bounding box intersects the current 16x16 chunk are placed.
- The same RNG is passed to every intersecting piece of every Village start
  referenced by that chunk. References are stored in a fastutil
  `LongOpenHashSet`, so overlapping Village starts are an extra ordering case
  that must not silently be ignored by an "exact" implementation.
- A `FeaturePoolElement` has a one-block point bounding box. If that point is
  in the current chunk, its configured feature receives the shared RNG
  directly.
- A legacy template uses the shared RNG only for
  `random.nextLong()` on each successfully placed
  `RandomizableContainerBlockEntity` inside the current chunk. This includes
  containers without a `LootTable`.
- Template palette selection and Village processors do not advance the shared
  RNG. Palette selection, `RuleProcessor`, `BlockRotProcessor`, and
  `BlockAgeProcessor` obtain coordinate-seeded local `Random` instances.
  Gravity and jigsaw replacement use no random calls. Village data-marker
  handling is empty.

The exact container order inside a piece is the processed template block
order. The manifest's `placement_index` preserves the relevant order.

## The 13 Village configured features

`Light exact` means shared-RNG advancement can be reproduced without
simulating the block world. Java `nextInt(1)` still advances the RNG.

| Configured feature | Shared-RNG behavior | Classification |
| --- | --- | --- |
| normal oak tree | Tree height/radius/offset and foliage calls occur only if terrain/substrate/clearance checks allow the tree | world-state dependent |
| pine tree | As above; foliage height adds a random call | world-state dependent |
| spruce tree | As above; foliage height and first spruce row add random calls | world-state dependent |
| acacia tree | As above; fork directions/lengths and possible second foliage attachment add branches | world-state dependent |
| hay block pile | Fixed shape-test floats, but successful placements consume one axis choice; grass-path support can consume `nextBoolean()` | world-state dependent |
| snow pile | Provider itself consumes no RNG, but a grass-path support block conditionally consumes `nextBoolean()` | world-state dependent (conditionally light) |
| melon pile | Same as snow pile | world-state dependent (conditionally light) |
| pumpkin pile | Each successful placement advances two `nextFloat()` calls in `WeightedStateProvider`; grass paths add a conditional boolean | world-state dependent |
| ice pile | Same as pumpkin pile (two weighted entries) | world-state dependent |
| cactus random patch | Always 10 attempts x 6 offset calls; every valid attempt additionally makes two nested `nextInt` calls in `ColumnPlacer` | world-state dependent |
| sweet berry random patch | Simple provider and placer consume no RNG; exactly 64 attempts x 6 offset calls | light exact |
| taiga grass random patch | Two `nextFloat()` calls select grass/fern once, then exactly 32 attempts x 6 offset calls | light exact |
| plains flower | Coordinate noise chooses a one- or two-call flower selection path, then exactly 64 x 6 offset calls; placement success consumes no RNG | light exact |

Tree failure/success depends on terrain height, water depth, substrate, template
blocks, preceding features, and clearance. Piles and cactus similarly inspect
and modify actual blocks. Reproducing those cases exactly requires a small
block-world simulation, not merely the heightmap.

## Two fixed oracle vectors

The target-level-1 SeedChecker oracle and the extracted template manifest were
used to transform loot-container positions and compare piece order by chunk:

| Seed / Village start | Pieces | Feature pieces | Loot chests | Loot chests with an earlier Feature in the same chunk |
| --- | ---: | ---: | ---: | ---: |
| `0`, chunk `(-25, 21)`, Taiga | 62 | 11 | 2 | 0 |
| `8040347553`, chunk `(9, 20)`, Savanna | 140 | 8 | 4 | 0 |

This does not prove the difficult case is impossible, but it shows that an
exact fast path which rejects only unsafe chest chunks is useful.

## Recommended implementation policy

1. Always generate exact Village layout and exact absolute container
   positions.
2. For a target chest, reconstruct its chunk's Village placement RNG
   (`decorationSeed + 40011`), then walk pieces and containers in exact stored
   order.
3. Advance through earlier containers with `nextLong()`.
4. Advance through earlier sweet-berry, taiga-grass, and plains-flower
   features with their lightweight exact algorithms.
5. If an earlier tree, pile, or cactus point exists in that chunk, mark the
   loot seed `unsafe/unresolved`; do not guess. Also mark chunks referencing
   more than one Village start unresolved until fastutil reference ordering
   and all starts are modeled.
6. The GUI can still expose exact chest coordinates for unresolved cases.
   Loot filtering should conservatively pass such candidates (or label them
   position-only) rather than produce false negatives.

This hybrid exact/position-only policy is substantially safer and faster than
implementing a partial tree or pile simulation and calling its loot output
exact.

## Integration sketch

A small API can keep uncertainty out of the loot-table generator:

```cpp
enum VillageLootSeedQuality16 {
    VILLAGE_LOOT_SEED_EXACT,
    VILLAGE_LOOT_SEED_UNRESOLVED_FEATURE,
    VILLAGE_LOOT_SEED_UNRESOLVED_OVERLAP,
};

struct VillageLootChest16 {
    VillageContainer16 container;
    uint64_t lootTableSeed;
    int quality;
};

bool assignVillageLootSeedsSingleStart16(
    QVector<VillageLootChest16> *out,
    const VillageLayout16& layout, uint64_t worldSeed,
    bool anotherVillageMayReferenceAChestChunk,
    QString *error = nullptr);
```

The explicit `SingleStart` name prevents the common path from being mistaken
for a proof that no second Village start references the same chunk.

```text
for each distinct chunk containing a loot-bearing Village container:
    rng = JavaRandom(worldSeed)
    a = rng.nextLong() | 1
    b = rng.nextLong() | 1
    decorationSeed =
        ((chunkX*16) * a + (chunkZ*16) * b) XOR worldSeed
    rng.setSeed(decorationSeed + 40011)
    unresolved = anotherVillageMayReferenceThisChunk

    for pieceIndex in 0 .. layout.pieces.size-1:
        piece = layout.pieces[pieceIndex]
        if piece bounding box does not intersect this chunk in X/Z:
            continue

        if piece is FEATURE:
            if feature is sweet berry, taiga grass, or plains flower:
                advanceSafeVillageFeature(rng, piece.feature, piece.pos)
            else:
                unresolved = true
            continue

        for container in containersByPiece[pieceIndex],
                         sorted by placementIndex:
            if container is not in this chunk:
                continue
            seed = rng.nextLong() // also for containers with no LootTable
            if container has a LootTable:
                emit(container, seed,
                     unresolved ? UNRESOLVED : EXACT)
```

For exact safe-feature advancement:

- sweet berry: 64 repetitions of bounds `8,8,4,4,8,8`;
- taiga grass: two `nextFloat`, then 32 repetitions of those bounds;
- plains flower: initialize a separate fixed simplex noise with Java seed
  `2345`, sample `(x/200.0,z/200.0)`, perform the one/two flower-selection
  calls, then 64 repetitions of bounds `7,7,3,3,7,7`.

`cubiomes` already has matching `perlinInit`/`sampleSimplex2D`, so the plains
flower path does not need a new noise implementation.

`lootcondition.cpp` should carry a `lootKnown` bit on
`GeneratedLootChest`/`LootSearchCacheChest`. Unknown seed is not the same as
an absent chest. Position filters remain exact. If an item rule includes an
unknown chest, the current search should conservatively keep the candidate
(tri-state `UNKNOWN`, treated as pass) instead of returning `false` from
`getStructureLoot`, which would create a false negative.
