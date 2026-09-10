# Java 26.2 ruined-portal and Bastion Loot

Select Java 26.2 in the world-version selector. The usual Loot editor then
uses the 26.2 tables automatically, for ruined portals in either dimension
and for all four Bastion chest types. Item-count ranges, AND/OR groups and
area totals remain available. Enchantments and levels can be selected on
eligible equipment as well as books; this includes Soul Speed equipment,
diamond pickaxes, and the treasure table's diamond spear. Chest-coordinate
filters are supported for Bastions, not portals.

## Local structure data

The official Java 26.2 client JAR must be installed locally. From the source
directory, run (adjust the JAR path if necessary):

```powershell
python tools/generate_loot_26_2.py --jar "$env:APPDATA\.minecraft\versions\26.2\26.2.jar"
.\dev-build.ps1 -NoRun -SkipTests
```

The extractor pins SHA-1 `2dc72797acbc1b63fc16a11c4ac393605f453754`. It emits
compact Loot/enchantment metadata and `build-structure-data/jigsaw-26.2.json`.
Rebuild deploys the latter alongside the application. Once deployed, no Java
runtime or running Minecraft instance is needed to search. The JAR, templates
and decompiled reference files are not included in Git. A missing manifest
disables Bastion Loot with an explanation rather than starting a Java helper.

## RNG and speed

The modern decoration RNG is Xoroshiro-backed `WorldgenRandom`. Its inherited
`nextLong()` consumes two 32-bit draws, not one raw Xoroshiro 64-bit draw.
Decoration seeds depend on the full world seed. Nonzero chest LootTableSeed
values then initialize a legacy Java LCG for the Loot table itself.

- Full-seed and family-block searches calculate each seed's chest contents.
- 48-bit-only searches do not decide modern Loot; they retain candidates.
- Bastion piece/chest layouts still depend on the lower 48 bits. A bounded
  per-worker cache reuses those layouts without reusing another seed's Loot.
- The village-style sampled rejection heuristic is not applied to 26.2 Loot.

Modern Bastion generation uses depth 6, the merged start pool, modern shuffle
draw counts, and the clipped Nether placement region. Portal Loot uses the
template bounding-box center chunk (not necessarily the start or chest chunk).
Structure decoration indices follow sorted vanilla registry keys, per
generation step. Enchantment options follow the release's ordered tags and
supported-item definitions rather than the 1.16 enum order.

## Limits

This implementation targets the default, unmodified Java 26.2 data pack.
Other modern releases are intentionally not enabled by similarity alone.
Portal biome/terrain viability remains the viewer's approximation. Full
terrain reconstruction, overlapping portal attempts, and terrain-related
missing portals are not resolved. Large-biome portal-category reconstruction
has not been validated. A rare zero LootTableSeed, which needs world-state
random-sequence information, is not reported as a known Loot match.

The existing self-contained Nether-entry/frame-completion condition remains
Java 1.16 only; porting modern portal Loot does not port that block-shape test.
Village terrain and unknown-state handling are unchanged.

Validation is deliberately small: build/source checks and console comparisons
of decoration/chest RNG against the official JAR, including negative
coordinates and equal-lower-48 seeds. This is not an exhaustive in-game
validation of every template or terrain case.

Reference classes inspected locally include `WorldgenRandom`,
`ChunkGenerator`, `ResourceManagerRegistryLoadTask`, `JigsawPlacement`,
`StructureTemplatePool`, `StructureTemplate`, `RuinedPortalStructure`,
`RuinedPortalPiece`, `LootContext` and `EnchantRandomlyFunction`.
