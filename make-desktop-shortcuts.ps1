$ErrorActionPreference = "Stop"

$sourceDir = $PSScriptRoot
$exe = Join-Path $sourceDir "build-dev-debug\debug\seed-quarry.exe"
$rebuildScript = Join-Path $sourceDir "rebuild-and-run.ps1"
$outputDir = Join-Path $sourceDir "desktop-shortcuts"
$windowsPowerShell = Join-Path $env:SystemRoot `
    "System32\WindowsPowerShell\v1.0\powershell.exe"

if (-not (Test-Path -LiteralPath $exe)) {
    throw "The built executable was not found. Run .\dev-build.ps1 -NoRun first."
}
if (-not (Test-Path -LiteralPath $rebuildScript)) {
    throw "The rebuild helper was not found: $rebuildScript"
}
if (-not (Test-Path -LiteralPath $windowsPowerShell)) {
    throw "Windows PowerShell was not found: $windowsPowerShell"
}
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$shell = New-Object -ComObject WScript.Shell

function Install-Shortcut {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$Target,
        [string]$Arguments = "",
        [string]$Description = ""
    )

    $path = Join-Path $outputDir $Name
    $shortcut = $shell.CreateShortcut($path)
    $shortcut.TargetPath = $Target
    $shortcut.Arguments = $Arguments
    $shortcut.WorkingDirectory = $sourceDir
    $shortcut.IconLocation = "$exe,0"
    $shortcut.Description = $Description
    $shortcut.WindowStyle = 1
    $shortcut.Save()
    return $path
}

$runShortcut = Install-Shortcut `
    -Name "Seed Quarry - Run.lnk" `
    -Target $exe `
    -Description "Start the current Seed Quarry development build"

$rebuildArguments = '-NoLogo -NoProfile -ExecutionPolicy Bypass -File "{0}"' `
    -f $rebuildScript
$rebuildShortcut = Install-Shortcut `
    -Name "Seed Quarry - Rebuild and Run.lnk" `
    -Target $windowsPowerShell `
    -Arguments $rebuildArguments `
    -Description "Rebuild Seed Quarry, deploy its Qt runtime, and start it"

Write-Host "Shortcuts created. Move these files to the desktop:"
Write-Host "  $runShortcut"
Write-Host "  $rebuildShortcut"
