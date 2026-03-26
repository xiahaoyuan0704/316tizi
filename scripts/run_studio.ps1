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
    throw "未找到 gim_studio.exe。请先执行: .\scripts\windows_build.ps1"
}

if (-not (Test-Path $ModelPath)) {
    throw "模型文件不存在: $ModelPath"
}

Write-Host "Run: $studio $ModelPath"
& $studio $ModelPath
