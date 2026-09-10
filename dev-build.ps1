param(
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug",
    [switch]$NoRun,
    [switch]$Reconfigure,
    [switch]$SkipTests,
    [switch]$SkipDeploy
)

$ErrorActionPreference = "Stop"

$sourceDir = $PSScriptRoot
$buildDir = Join-Path $sourceDir "build-dev-$Configuration"

& (Join-Path $sourceDir "check-source.ps1")

function Find-QtTool {
    param(
        [string]$ToolName,
        [string[]]$Candidates
    )

    $command = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($candidate in $Candidates) {
        $match = Get-Item $candidate -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($match) {
            return $match.FullName
        }
    }

    return $null
}

$qmakeCandidates = @()
if ($env:QTDIR) {
    $qmakeCandidates += (Join-Path $env:QTDIR "bin\qmake.exe")
}
$qmakeCandidates += "C:\Qt\6.8.*\mingw_64\bin\qmake.exe"
$qmakeCandidates += "C:\Qt\6.*\mingw_64\bin\qmake.exe"

$qmake = Find-QtTool "qmake.exe" $qmakeCandidates
if (-not $qmake) {
    throw @"
Qt 6 MinGW was not found.
Install "Qt 6.x / MinGW 64-bit" and
"Developer and Designer Tools / MinGW 13.1 64-bit" with Qt Online Installer.
The script auto-detects a standard C:\Qt installation.
"@
}

$qtRoot = Split-Path (Split-Path $qmake -Parent) -Parent
$makeCandidates = @(
    (Join-Path $qtRoot "bin\mingw32-make.exe"),
    "C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe",
    "C:\Qt\Tools\mingw*_64\bin\mingw32-make.exe"
)
$make = Find-QtTool "mingw32-make.exe" $makeCandidates
if (-not $make) {
    throw "mingw32-make.exe was not found. Add MinGW 13.1 64-bit with Qt Online Installer."
}

$mingwBin = Split-Path $make -Parent
$qtBin = Split-Path $qmake -Parent
$env:Path = "$qtBin;$mingwBin;$env:Path"
$env:QT_PLUGIN_PATH = Join-Path $qtRoot "plugins"

if (-not $SkipTests) {
    & $make -C (Join-Path $sourceDir "cubiomes") test-versions
    if ($LASTEXITCODE -ne 0) {
        throw "Cubiomes tests failed (exit $LASTEXITCODE)"
    }
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$makefile = Join-Path $buildDir "Makefile"

if ($Reconfigure -or -not (Test-Path $makefile)) {
    Push-Location $buildDir
    try {
        $configAdd = "CONFIG+=$Configuration"
        $configRemove = if ($Configuration -eq "debug") { "CONFIG-=release" } else { "CONFIG-=debug" }
        & $qmake $configAdd $configRemove (Join-Path $sourceDir "seed-quarry.pro")
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed (exit $LASTEXITCODE)"
        }
    }
    finally {
        Pop-Location
    }
}

# The Village feature simulator makes a few C++ translation units large
# enough that two concurrent MinGW compilers can exhaust memory on this
# workstation. A single job is slower but keeps rebuild/run reliable.
$jobs = 1
& $make -C $buildDir "-j$jobs"
if ($LASTEXITCODE -ne 0) {
    throw "Build failed (exit $LASTEXITCODE)"
}

$exe = Join-Path $buildDir "$Configuration\seed-quarry.exe"
if (-not (Test-Path $exe)) {
    throw "Build completed but the executable was not found: $exe"
}

$structureManifest = Join-Path $sourceDir "build-structure-data\jigsaw-1.16.1.json"
$modernManifest = Join-Path $sourceDir "build-structure-data\jigsaw-26.2.json"
if (Test-Path $modernManifest) {
    Copy-Item -LiteralPath $modernManifest -Destination (Split-Path $exe -Parent) -Force
}
if (Test-Path $structureManifest) {
    Copy-Item -LiteralPath $structureManifest `
        -Destination (Split-Path $exe -Parent) -Force
}
else {
    Write-Warning @"
The generated 1.16.1 jigsaw manifest was not found.
Bastion and village chest-layout filters will remain disabled.
"@
}

$deploy = Join-Path $qtBin "windeployqt.exe"
if (-not $SkipDeploy) {
    if (-not (Test-Path $deploy)) {
        throw "windeployqt.exe was not found next to qmake: $deploy"
    }
    # The installed MinGW Qt runtime uses the release-named Qt6*.dll files
    # for both local configurations. Copy Qt, platform plugins, and compiler
    # runtimes beside the executable so it can be started by double-clicking.
    & $deploy --release --compiler-runtime $exe
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed (exit $LASTEXITCODE)"
    }
}

Write-Host "Build completed: $exe"
if (-not $NoRun) {
    & $exe
}
