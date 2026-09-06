# Seed Quarry

Seed Quarry is an independent GPLv3 source-code fork of
[Seed Atlas](https://github.com/DUzzL/Seed-Atlas) by DUzzL. Seed Atlas is
itself based on [Cubiomes Viewer](https://github.com/Cubitect/cubiomes-viewer)
by Cubitect.

Most of the viewer interface, biome and structure map, and original
seed-search workflow come from those upstream projects. Seed Quarry retains
their copyright and licence notices and does not claim those parts as
original work. It is not an official release of either upstream project.

## Project lineage and references

```text
Cubiomes Viewer
    -> Seed Atlas
        -> Seed Quarry

MineMap 1.0.26 + SeedFinding libraries
    -> references for portions of the Java 1.16 Loot implementation
```

Some fixed-structure Loot behavior and validation data were implemented with
reference to [MineMap](https://github.com/hube12/MineMap) and its pinned
SeedFinding Java dependencies. Seed Quarry does not bundle the MineMap
application or its user interface. Exact versions and implementation notes
are recorded in [LOOT_INTEGRATION_JA.md](LOOT_INTEGRATION_JA.md), with licence
details in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Seed Quarry additions

- Searchable structure-generation variants, including ruined-portal
  placement details.
- Java 1.16 Loot filters for desert pyramids, shipwrecks, buried treasure,
  ruined portals, villages, and bastion remnants.
- Item-count, AND/OR, per-chest, per-structure, area-total, enchantment, and
  chest-coordinate rules.
- Exact and explicitly labelled fast sampled Loot modes for 48-bit searches.
- Experimental village and bastion layout and chest-position analysis.
- Resizable condition dialogs, development scripts, Windows packaging, and a
  partial Japanese translation.

The underlying interactive map, much of the existing condition system, and
general seed-search functionality remain upstream work.

## Search accuracy

`Exact Loot search` is the exhaustive choice for 48-bit family-block searches
and checks every eligible upper-16-bit seed.

`Fast sampled Loot search` is intended for quickly finding one useful result.
For village Loot it may reject an entire 48-bit family after testing the first
upper-16-bit seed that satisfies all non-Loot conditions. It can therefore
miss matches. The application exposes this as a separate mode and describes
the trade-off in the selector tooltip. Unsafe logical combinations disable
village-family sampling automatically.

Village generation still has deliberately conservative `unknown` cases when
the Java 1.16 block state or shared RNG stream cannot be proved. Those cases
are not silently treated as exact matches.

## Build on Windows

Install Qt 6 with its 64-bit MinGW component, then run from PowerShell:

```powershell
.\dev-build.ps1
```

The script performs source checks, incrementally builds the application,
deploys the required Qt runtime files, and starts Seed Quarry. Build without
starting it with:

```powershell
.\dev-build.ps1 -NoRun
```

The first build, or a build after changing the qmake project, can be forced to
reconfigure with:

```powershell
.\dev-build.ps1 -Reconfigure
```

See [buildguide.md](buildguide.md) for other platforms and manual builds.

## Tests

The lightweight source and Loot checks used during development are:

```powershell
.\check-source.ps1
.\test-loot.ps1 -SkipAppBuild
```

The Java 1.16 village and bastion layout features require the generated
`jigsaw-1.16.1.json` structure-data file beside the executable. The local
development script deploys it when it exists. Generation notes and tools are
kept in `tools/`; this large derived file is not committed to the repository.
Other viewer and fixed-structure Loot functions build without it.

## Project status

The fork is under active development. Java 1.16 is the priority for the
structure-Loot work; support shown for other Minecraft versions mostly comes
from the underlying viewer and `cubiomes`. See
[VERSION_SUPPORT.md](VERSION_SUPPORT.md) for the broader version matrix and
`docs/` for implementation notes.

## Licence and attribution

Seed Quarry is distributed under GPLv3. See [LICENSE](LICENSE),
[LEGAL_NOTICE.md](LEGAL_NOTICE.md), and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Contributions must preserve
the notices and licence obligations of upstream and referenced components.

Seed Quarry is not an official Minecraft product and is not approved by or
associated with Mojang or Microsoft.
