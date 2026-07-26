param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Seed,

    [Parameter(Position = 1)]
    [int]$ChunkX,

    [Parameter(Position = 2)]
    [int]$ChunkZ,

    [int]$ScanRadius = -1,

    [switch]$Loot
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$oracleJar = Join-Path $repoRoot `
    ".reference-seed-checker-1.16.1\seed-checker-1.2.0-1.16.1.jar"
$source = Join-Path $PSScriptRoot "VillageOracle1161.java"
$buildDir = Join-Path $repoRoot "build-structure-data\village-oracle"
$runtimeJar = Join-Path $buildDir "seed-checker-1.2.0-1.16.1.jar"
$manifest = Join-Path $repoRoot "build-structure-data\jigsaw-1.16.1.json"

if (-not (Test-Path -LiteralPath $oracleJar -PathType Leaf)) {
    throw "SeedChecker 1.16.1 jar was not found: $oracleJar"
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
# ZipFS tries to reopen/canonicalize its class-path jar while closing.  A
# private copy in the writable build directory avoids failures when the
# reference download is held read-only by the surrounding workspace.
Copy-Item -LiteralPath $oracleJar -Destination $runtimeJar -Force
& javac -proc:none -encoding UTF-8 -cp $runtimeJar `
    -d $buildDir $source
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$classPath = "$buildDir;$runtimeJar"
if ($Loot -and $ScanRadius -ge 0) {
    throw "-Loot and -ScanRadius cannot be used together."
}

# SeedChecker may persist intermediate region data relative to the process
# working directory.  Keep every such side effect inside the disposable
# oracle build directory instead of the repository root.
Push-Location -LiteralPath $buildDir
try {
    if ($Loot) {
        if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
            throw "Jigsaw manifest was not found: $manifest"
        }
        & java -Xmx1024m -cp $classPath VillageOracle1161 `
            --loot $Seed $ChunkX $ChunkZ $manifest
    } elseif ($ScanRadius -ge 0) {
        & java -Xmx1024m -cp $classPath VillageOracle1161 `
            --scan $Seed $ScanRadius
    } else {
        & java -Xmx1024m -cp $classPath VillageOracle1161 `
            $Seed $ChunkX $ChunkZ
    }
    $javaExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $javaExitCode
