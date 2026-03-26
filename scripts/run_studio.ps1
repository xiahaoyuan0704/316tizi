param(
    [string]$ModelPath = "sample\demo.gim",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$candidates = @(
    ".\build\gim_studio.exe",
    ".\build\$Configuration\gim_studio.exe"
)

$studio = $null
foreach ($c in $candidates) {
    if (Test-Path $c) {
        $studio = $c
        break
    }
}

if (-not $studio) {
    throw "gim_studio.exe not found. Run: .\\scripts\\windows_build.ps1"
}

if (-not (Test-Path $ModelPath)) {
    throw "Model file not found: $ModelPath"
}

Write-Host "Run: $studio $ModelPath"
& $studio $ModelPath
