param(
    [string]$Configuration = "Release",
    [switch]$DisableStudio,
    [string]$Generator = "",
    [string]$Platform = "x64"
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

    throw "cmake.exe not found. Install CMake (Add CMake to PATH) or install Visual Studio C++ CMake tools."
}

function Resolve-VsGenerator {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $version = & $vswhere -latest -products * -property catalog_productLineVersion
        if ($version -and $version.Trim() -eq "2022") {
            return "Visual Studio 17 2022"
        }
        if ($version -and $version.Trim() -eq "2019") {
            return "Visual Studio 16 2019"
        }
    }
    return ""
}

function Invoke-Native([string]$File, [string[]]$Args) {
    & $File @Args
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $File $($Args -join ' ')"
    }
}

$cmakeExe = Resolve-CMakePath
Write-Host "Using CMake: $cmakeExe"

$sourceDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $sourceDir "build"

$configureArgs = @("-S", $sourceDir, "-B", $buildDir)
if ($DisableStudio) {
    $configureArgs += "-DGIM_ENABLE_STUDIO=OFF"
}

if ($Generator) {
    $configureArgs += @("-G", $Generator)
    if ($Generator -like "Visual Studio*") {
        $configureArgs += @("-A", $Platform)
    }
} else {
    $vsGenerator = Resolve-VsGenerator
    if ($vsGenerator) {
        Write-Host "Auto generator: $vsGenerator ($Platform)"
        $configureArgs += @("-G", $vsGenerator, "-A", $Platform)
    } else {
        Write-Host "Auto generator: default CMake generator"
    }
}

Invoke-Native $cmakeExe $configureArgs
Invoke-Native $cmakeExe @("--build", $buildDir, "--config", $Configuration)

Write-Host "Build done."
