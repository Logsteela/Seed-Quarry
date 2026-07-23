param()

$ErrorActionPreference = "Stop"

$sourceDir = $PSScriptRoot
$uiPath = Join-Path $sourceDir "src\conditiondialog.ui"
$dialogPath = Join-Path $sourceDir "src\conditiondialog.cpp"
$searchPath = Join-Path $sourceDir "src\search.cpp"
$headerPath = Join-Path $sourceDir "src\search.h"
$buildScriptPath = Join-Path $sourceDir "dev-build.ps1"

function Assert-SourceCheck {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$uiText = $strictUtf8.GetString([System.IO.File]::ReadAllBytes($uiPath))
Assert-SourceCheck (-not $uiText.Contains([char]0xfffd)) `
    "conditiondialog.ui contains an invalid UTF-8 replacement character."

try {
    [xml]$uiXml = $uiText
}
catch {
    throw "conditiondialog.ui is not valid XML: $($_.Exception.Message)"
}

$namedObjects = @(
    $uiXml.SelectNodes(
        "//widget[@name] | //layout[@name] | //spacer[@name] | //action[@name]"
    )
)
$objectNames = @($namedObjects | ForEach-Object { [string]$_.name })
$duplicateNames = @(
    $objectNames |
        Group-Object |
        Where-Object Count -gt 1 |
        ForEach-Object Name
)
Assert-SourceCheck ($duplicateNames.Count -eq 0) `
    "Duplicate Qt object names: $($duplicateNames -join ', ')"

$requiredObjects = @(
    "comboVillageRotation",
    "comboBastionRotation",
    "comboIglooOrientation",
    "comboIglooSize",
    "comboAncientStart",
    "comboAncientRotation",
    "comboChambersStart",
    "comboChambersRotation",
    "comboTempleOrientation",
    "pageAncientCity",
    "pageChambers",
    "pageTemple"
)
$missingObjects = @($requiredObjects | Where-Object { $_ -notin $objectNames })
Assert-SourceCheck ($missingObjects.Count -eq 0) `
    "Required Qt objects are missing: $($missingObjects -join ', ')"

$stackedWidget = $uiXml.SelectSingleNode("//widget[@name='stackedWidget']")
Assert-SourceCheck ($null -ne $stackedWidget) "stackedWidget was not found."
$directPages = @($stackedWidget.SelectNodes("./widget") | ForEach-Object { [string]$_.name })
$requiredPages = @("pageAncientCity", "pageChambers", "pageTemple")
$nestedPages = @($requiredPages | Where-Object { $_ -notin $directPages })
Assert-SourceCheck ($nestedPages.Count -eq 0) `
    "Pages must be direct children of stackedWidget: $($nestedPages -join ', ')"

$dialogText = Get-Content -LiteralPath $dialogPath -Raw -Encoding UTF8
$uiReferences = @(
    [regex]::Matches($dialogText, "ui->([A-Za-z_][A-Za-z0-9_]*)") |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne "setupUi" } |
        Sort-Object -Unique
)
$missingReferences = @($uiReferences | Where-Object { $_ -notin $objectNames })
Assert-SourceCheck ($missingReferences.Count -eq 0) `
    "conditiondialog.cpp refers to missing Qt objects: $($missingReferences -join ', ')"

$searchText = Get-Content -LiteralPath $searchPath -Raw -Encoding UTF8
$headerText = Get-Content -LiteralPath $headerPath -Raw -Encoding UTF8
$variantDependencies = @(
    "DEP_VILLAGE_ROTATION",
    "DEP_BASTION_ROTATION",
    "DEP_IGLOO_ORIENTATION",
    "DEP_IGLOO_SIZE",
    "DEP_ANCIENT_START",
    "DEP_ANCIENT_ROTATION",
    "DEP_CHAMBERS_START",
    "DEP_CHAMBERS_ROTATION",
    "DEP_TEMPLE_ORIENTATION"
)
foreach ($dependency in $variantDependencies) {
    $count = [regex]::Matches(
        "$headerText`n$searchText`n$dialogText",
        "\b$dependency\b"
    ).Count
    Assert-SourceCheck ($count -eq 4) `
        "$dependency should have one declaration and three uses; found $count."
}

$tokens = $null
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
    $buildScriptPath,
    [ref]$tokens,
    [ref]$parseErrors
) | Out-Null
Assert-SourceCheck ($parseErrors.Count -eq 0) `
    "dev-build.ps1 has PowerShell syntax errors: $($parseErrors.Message -join '; ')"

Write-Host "Source checks passed:"
Write-Host "  UTF-8 and conditiondialog.ui XML"
Write-Host "  unique Qt object names and C++ UI references"
Write-Host "  structure-variant dependency wiring"
Write-Host "  dev-build.ps1 syntax"
