param(
    [string]$Configuration = "Release",
    [switch]$DisableStudio
)

$ErrorActionPreference = "Stop"

function Resolve-CMakePath {
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        return $cmake.Source
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $candidates = @(
        "C:\Program Files\CMake\bin\cmake.exe",
        "C:\Program Files (x86)\CMake\bin\cmake.exe"
    )

    foreach ($c in $candidates) {
        if (Test-Path $c) {
            return $c
        }
    }

    throw "未找到 cmake.exe。请安装 CMake（勾选 Add CMake to system PATH），或安装 Visual Studio 的 C++ CMake 工具。"
}

$cmakeExe = Resolve-CMakePath
Write-Host "Using CMake: $cmakeExe"

$sourceDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $sourceDir "build"

$configureArgs = @("-S", $sourceDir, "-B", $buildDir)
if ($DisableStudio) {
    $configureArgs += "-DGIM_ENABLE_STUDIO=OFF"
}

& $cmakeExe @configureArgs
& $cmakeExe --build $buildDir --config $Configuration

Write-Host "Build done."
