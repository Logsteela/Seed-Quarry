# Contributing

Contributions and reproducible bug reports are welcome. This is an
experimental fork, so small changes with a clear accuracy claim are easier to
review than broad rewrites.

## Development rules

- Keep source UI strings in English and wrap user-visible strings with Qt's
  translation functions.
- Update `translations/seed-atlas_ja.ts` when changing translated controls.
- Preserve the numeric values and serialized form of existing search and Loot
  settings unless a migration is included.
- State whether a search optimization is exhaustive or can miss matches.
  Approximate behaviour must be opt-in and described in a tooltip.
- Do not treat an unresolved Village Loot result as an exact match.
- Preserve upstream copyright notices and add attribution for newly imported
  or ported code.

## Before submitting

On Windows, run:

```powershell
.\check-source.ps1
.\dev-build.ps1 -NoRun -SkipTests
```

For Loot changes, also run the focused suite when practical:

```powershell
.\test-loot.ps1 -SkipAppBuild
```

Include the Minecraft edition/version, search type, conditions, and a minimal
reproduction session with accuracy-related bug reports.
