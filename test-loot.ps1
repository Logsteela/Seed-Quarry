param(
    [switch]$SkipAppBuild
)

$ErrorActionPreference = "Stop"

$sourceDir = $PSScriptRoot
$appBuildDir = Join-Path $sourceDir "build-dev-debug"
$testBuildDir = Join-Path $sourceDir "build-tests"

if (-not $SkipAppBuild) {
    & (Join-Path $sourceDir "dev-build.ps1") -NoRun
}

$qmake = Get-Item "C:\Qt\6.*\mingw_64\bin\qmake.exe" `
    -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1
$make = Get-Item "C:\Qt\Tools\mingw*_64\bin\mingw32-make.exe" `
    -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $qmake -or -not $make) {
    throw "Qt 6 MinGW tools were not found under C:\Qt."
}

$qtRoot = Split-Path (Split-Path $qmake.FullName -Parent) -Parent
$qtBin = Split-Path $qmake.FullName -Parent
$mingwBin = Split-Path $make.FullName -Parent
$env:Path = "$qtBin;$mingwBin;$env:Path"
$env:QT_PLUGIN_PATH = Join-Path $qtRoot "plugins"

New-Item -ItemType Directory -Path $testBuildDir -Force | Out-Null
Push-Location $testBuildDir
try {
    & $qmake.FullName (Join-Path $sourceDir "tests\lootcondition_tests.pro")
    if ($LASTEXITCODE -ne 0) {
        throw "Loot test qmake failed (exit $LASTEXITCODE)"
    }
    & $make.FullName "-j$([Math]::Max(1, [Environment]::ProcessorCount))"
    if ($LASTEXITCODE -ne 0) {
        throw "Loot test build failed (exit $LASTEXITCODE)"
    }
}
finally {
    Pop-Location
}

$unitTest = Join-Path $testBuildDir "release\lootcondition_tests.exe"
& $unitTest
if ($LASTEXITCODE -ne 0) {
    throw "Loot unit tests failed (exit $LASTEXITCODE)"
}

$app = Join-Path $appBuildDir "debug\seed-atlas.exe"
if (-not (Test-Path $app)) {
    throw "Seed Atlas debug build was not found: $app"
}

function Test-HeadlessSession {
    param(
        [string]$Session,
        [string[]]$ExpectedSeeds
    )

    $name = [IO.Path]::GetFileNameWithoutExtension($Session)
    $resultPath = Join-Path $testBuildDir "$name.results.txt"
    if (Test-Path $resultPath) {
        Remove-Item -LiteralPath $resultPath
    }

    & $app --nogui "--session=$Session" "--out=$resultPath"
    Start-Sleep -Milliseconds 100
    $deadline = (Get-Date).AddSeconds(30)
    do {
        $running = @(
            Get-Process seed-atlas -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $app }
        )
        if ($running.Count -eq 0) {
            break
        }
        Start-Sleep -Milliseconds 50
    } while ((Get-Date) -lt $deadline)
    if ($running.Count -ne 0) {
        throw "Headless integration timed out for $Session."
    }
    if (-not (Test-Path $resultPath)) {
        throw "Headless result file was not created for $Session."
    }
    $actual = @(
        Get-Content -LiteralPath $resultPath |
        Where-Object { $_ -match "^-?[0-9]+$" }
    )
    if (($actual -join "`n") -ne ($ExpectedSeeds -join "`n")) {
        throw "Unexpected seeds for $Session. Expected: $ExpectedSeeds; actual: $actual"
    }
}

Push-Location $sourceDir
try {
    Test-HeadlessSession `
        "tests\loot_structure_integration_session.txt" `
        @("3515201313347228787")
    Test-HeadlessSession `
        "tests\loot_integration_session.txt" `
        @("3515201313347228787")
    Test-HeadlessSession `
        "tests\loot_integration_fail_session.txt" `
        @()
}
finally {
    Pop-Location
}

Write-Host "All Loot unit and headless integration tests passed."
