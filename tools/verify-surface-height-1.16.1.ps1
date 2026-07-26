$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$oracleJar = Join-Path $repoRoot `
    ".reference-seed-checker-1.16.1\seed-checker-1.2.0-1.16.1.jar"
$buildDir = Join-Path $repoRoot `
    "build-structure-data\surface-height-oracle"
$runtimeJar = Join-Path $buildDir `
    "seed-checker-1.2.0-1.16.1.jar"
$cProbe = Join-Path $buildDir "surface_height_probe.exe"

if (-not (Test-Path -LiteralPath $oracleJar -PathType Leaf)) {
    throw "SeedChecker 1.16.1 jar was not found: $oracleJar"
}

$gccCommand = Get-Command gcc.exe -ErrorAction SilentlyContinue
$gcc = if ($gccCommand) { $gccCommand.Source } else { $null }
if (-not $gcc) {
    $gcc = Get-ChildItem `
        -Path "C:\Qt\Tools\mingw*_64\bin\gcc.exe" `
        -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $gcc) {
    throw "gcc.exe was not found. Install Qt's MinGW component."
}

$mingwBin = Split-Path -Parent $gcc
$env:Path = "$mingwBin;$env:Path"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Copy-Item -LiteralPath $oracleJar -Destination $runtimeJar -Force

$cubiomes = Join-Path $repoRoot "cubiomes"
$cSources = @(
    (Join-Path $PSScriptRoot "surface_height_probe.c"),
    (Join-Path $cubiomes "noise.c"),
    (Join-Path $cubiomes "biomes.c"),
    (Join-Path $cubiomes "layers.c"),
    (Join-Path $cubiomes "biomenoise.c"),
    (Join-Path $cubiomes "generator.c")
)
& $gcc -std=c11 -O2 -Wall -Wextra -fwrapv `
    -I $cubiomes -o $cProbe @cSources -lm
if ($LASTEXITCODE -ne 0) {
    throw "C surface-height probe compilation failed (exit $LASTEXITCODE)"
}

$javaSource = Join-Path $PSScriptRoot "SurfaceHeightOracle1161.java"
& javac -proc:none -encoding UTF-8 -cp $runtimeJar `
    -d $buildDir $javaSource
if ($LASTEXITCODE -ne 0) {
    throw "Java surface-height oracle compilation failed (exit $LASTEXITCODE)"
}

# Includes the seed-0 village start at chunk (-25, 21). Its first piece has
# box centre (-410, 327), whose official start height is 66.
$arguments = @(
    "0",
    "-410", "327",
    "-399", "340",
    "-405", "353",
    "-386", "338",
    "-421", "319",
    "-400", "336",
    "0", "0",
    "100", "100",
    "-1", "-1",
    "-4", "-4",
    "-5", "-5"
)
$expected = @(
    "H|0|-410|327|66",
    "H|0|-399|340|66",
    "H|0|-405|353|66",
    "H|0|-386|338|65",
    "H|0|-421|319|66",
    "H|0|-400|336|64",
    "H|0|0|0|72",
    "H|0|100|100|63",
    "H|0|-1|-1|71",
    "H|0|-4|-4|70",
    "H|0|-5|-5|70"
)

$classPath = "$buildDir;$runtimeJar"
$javaOutput = @(
    & java -Xmx1024m -cp $classPath `
        SurfaceHeightOracle1161 @arguments
)
if ($LASTEXITCODE -ne 0) {
    throw "Java surface-height oracle failed (exit $LASTEXITCODE)"
}
$cOutput = @(& $cProbe @arguments)
if ($LASTEXITCODE -ne 0) {
    throw "C surface-height probe failed (exit $LASTEXITCODE)"
}

$oracleDifference = Compare-Object $expected $javaOutput
if ($oracleDifference) {
    $oracleDifference | Format-Table | Out-String | Write-Host
    throw "SeedChecker output differs from the recorded 1.16.1 sample."
}
$implementationDifference = Compare-Object $javaOutput $cOutput
if ($implementationDifference) {
    $implementationDifference | Format-Table | Out-String | Write-Host
    throw "C heights differ from SeedChecker."
}

Write-Host `
    "Surface height verification passed: $($cOutput.Count) exact samples."
