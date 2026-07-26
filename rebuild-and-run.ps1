$ErrorActionPreference = "Stop"

try {
    & (Join-Path $PSScriptRoot "dev-build.ps1")
}
catch {
    Write-Host
    Write-Host "Seed Atlas rebuild failed." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host
    Read-Host "Press Enter to close"
    exit 1
}
