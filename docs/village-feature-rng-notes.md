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
simulating the block world. `Compact-state exact` means it can be reproduced
when the extracted Village blocks and sampled surface around the feature are
enough to prove every world-state branch. Java `nextInt(1)` still advances
the RNG.

| Configured feature | Shared-RNG behavior | Classification |
| --- | --- | --- |
| normal oak tree | Tree height/radius/offset and foliage calls occur only if terrain/substrate/clearance checks allow the tree | compact-state exact |
| pine tree | As above; foliage height adds a random call | compact-state exact |
| spruce tree | As above; foliage height and first spruce row add random calls | compact-state exact |
| acacia tree | As above; fork directions/lengths and possible second foliage attachment add branches | compact-state exact |
| hay block pile | Fixed shape-test floats, successful placements consume one axis choice, and grass-path support can consume `nextBoolean()` | compact-state exact |
| snow pile | Provider itself consumes no RNG, but a grass-path support block conditionally consumes `nextBoolean()` | compact-state exact |
| melon pile | Same as snow pile | compact-state exact |
| pumpkin pile | Each successful placement advances two `nextFloat()` calls in `WeightedStateProvider`; grass paths add a conditional boolean | compact-state exact |
| ice pile | Same as pumpkin pile (two weighted entries) | compact-state exact |
| cactus random patch | Always 10 attempts x 6 offset calls; every valid attempt additionally makes two nested `nextInt` calls in `ColumnPlacer` | compact-state exact |
| sweet berry random patch | Simple provider and placer consume no RNG; exactly 64 attempts x 6 offset calls | light exact |
| taiga grass random patch | Two `nextFloat()` calls select grass/fern once, then exactly 32 attempts x 6 offset calls | light exact |
| plains flower | Coordinate noise chooses a one- or two-call flower selection path, then exactly 64 x 6 offset calls; placement success consumes no RNG | light exact |

Tree failure/success depends on terrain height, water depth, substrate, template
blocks, preceding features, and clearance. Piles and cactus similarly inspect
and modify actual blocks. The implementation therefore extracts grass paths
and coarse block kinds from the official 1.16.1 templates, transforms them in
piece order, and samples `WORLD_SURFACE_WG` only around feature points. It
simulates blocks placed by the feature while consuming the shared RNG.

It deliberately returns `UNRESOLVED_FEATURE` instead of guessing when that
compact model cannot prove a branch. Current conservative cases include a
feature query below the sampled top surface (where caves/carvers matter),
zombie-processor blocks whose coarse collision category can actually change,
and block kinds whose material behavior was not extracted. Feature placement
across a chunk edge, water at sea level, and the runtime tree heightmap raised
by earlier template/path blocks are modeled explicitly. The manifest retains
the original block identity as well as its coarse category, so unaffected
zombie-template blocks no longer become blanket unknown. This keeps loot
search free of false negatives while resolving many formerly conservative
cases.

## Oracle vectors

The target-level-1 SeedChecker oracle and the extracted template manifest were
used to transform loot-container positions and compare piece order by chunk:

| Seed / Village start | Pieces | Feature pieces | Loot chests | Loot chests with an earlier Feature in the same chunk |
| --- | ---: | ---: | ---: | ---: |
| `0`, chunk `(-25, 21)`, Taiga | 62 | 11 | 2 | 0 |
| `8040347553`, chunk `(9, 20)`, Savanna | 140 | 8 | 4 | 0 |

This does not prove the difficult case is impossible, but it shows that an
exact fast path which rejects only unsafe chest chunks is useful.

An actual target-level-1 Minecraft oracle was also run for seed
`8709371129873690708`, Snowy Village start chunk `(-371,-396)`. The four
loot-table seeds reproduced by the C++ implementation are:

| Chest position | LootTableSeed |
| --- | ---: |
| `(-5918,75,-6351)` | `-3285011792938035519` |
| `(-5924,75,-6350)` | `2146034468891856845` |
| `(-5980,77,-6331)` | `8560071340488466059` |
| `(-5974,76,-6325)` | `5135956710036044922` |

Two interaction-heavy Taiga checks additionally match the SeedChecker 1.16.1
oracle exactly:

- seed `3962023812499842531`, start chunk `(236,328)`: a preceding template
  raises the runtime tree base at sea level;
- seed `5718060553726506393`, start chunk `(275,-303)`: cross-chunk pumpkin
  pile and spruce-feature queries;
- seed `3026716864276998616`, start chunk `(374,-234)`: nine chests, including
  a pine tree beside a zombie meeting-point template. All nine C++ loot seeds
  equal the Java oracle values.

A deterministic 100-layout probe currently resolves 332 of 337 loot chests.
The five conservative results are cactus/pumpkin queries that need deep
terrain or exact partial-block behavior. Among 1,000 viable Village starts in
the probe sequence, only four starts remain unresolved, all because cactus
queries reach terrain below the known top surface.

`tools/VillageFeatureRngOracle1161.java` is a small independent
`java.util.Random` oracle for flat, structure-free branches. It covers all
four trees, hay, weighted piles, and cactus without loading Minecraft or
SeedChecker. Its fixed vectors are asserted by
`tools/village_loot_seed_probe.cpp`.

## Recommended implementation policy

1. Always generate exact Village layout and exact absolute container
   positions.
2. For a target chest, reconstruct its chunk's Village placement RNG
   (`decorationSeed + 40011`), then walk pieces and containers in exact stored
   order.
3. Advance through earlier containers with `nextLong()`.
4. Advance through lightweight features directly. For trees, piles, and
   cactus, use the compact block-state simulation; mark only later containers
   unresolved if a required state is not provable.
5. Detect competing Village RNG consumers per chest chunk. Mark only the
   affected chunks unresolved, rather than invalidating every chest in the
   target Village.
6. The GUI can still expose exact chest coordinates for unresolved cases.
   Loot filtering should conservatively pass such candidates (or label them
   position-only) rather than produce false negatives.

This hybrid exact/position-only policy keeps the fast proven branches while
making uncertainty explicit instead of calling a guessed tree or pile result
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
    const QVector<Pos>& overlappingChestChunks,
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
    unresolved = overlappingVillageRngChunks.contains(thisChunk)

    for pieceIndex in 0 .. layout.pieces.size-1:
        piece = layout.pieces[pieceIndex]
        if piece bounding box does not intersect this chunk in X/Z:
            continue

        if piece is FEATURE:
            if not advanceVillageFeature(rng, compactWorld, piece):
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

`lootcondition.cpp` carries a `lootKnown` bit on
`GeneratedLootChest`/`LootSearchCacheChest`. Unknown seed is not the same as
an absent chest. Position filters remain exact. If an item rule includes an
unknown chest, the current search should conservatively keep the candidate
(tri-state `UNKNOWN`, treated as pass) instead of returning `false` from
`getStructureLoot`, which would create a false negative.
