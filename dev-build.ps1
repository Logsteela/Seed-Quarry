param(
    [ValidateSet("debug", "release")]
    [string]$Configuration = "debug",
    [switch]$NoRun,
    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"

$sourceDir = $PSScriptRoot
$buildDir = Join-Path $sourceDir "build-dev-$Configuration"

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

$qmake = Find-QtTool "qmake.exe" $qmakeCandidates
if (-not $qmake) {
    throw @"
Qt 6.8 MinGW was not found.
Install "Qt 6.8.x / MinGW 64-bit" and
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

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$makefile = Join-Path $buildDir "Makefile"

if ($Reconfigure -or -not (Test-Path $makefile)) {
    Push-Location $buildDir
    try {
        $configAdd = "CONFIG+=$Configuration"
        $configRemove = if ($Configuration -eq "debug") { "CONFIG-=release" } else { "CONFIG-=debug" }
        & $qmake $configAdd $configRemove (Join-Path $sourceDir "seed-atlas.pro")
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed (exit $LASTEXITCODE)"
        }
    }
    finally {
        Pop-Location
    }
}

$jobs = [Math]::Max(1, [Environment]::ProcessorCount)
& $make -C $buildDir "-j$jobs"
if ($LASTEXITCODE -ne 0) {
    throw "Build failed (exit $LASTEXITCODE)"
}

$exe = Join-Path $buildDir "$Configuration\seed-atlas.exe"
if (-not (Test-Path $exe)) {
    throw "Build completed but the executable was not found: $exe"
}

Write-Host "Build completed: $exe"
if (-not $NoRun) {
    & $exe
}
