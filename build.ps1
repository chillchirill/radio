param(
    [switch]$Clean,
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$BuildType = "Release",
    [switch]$Reconfigure,
    [string]$Target
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "build"
$vcpkgRoot = Join-Path $root "vcpkg"
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
$toolchain = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"

function Remove-BuildDirectoryIfExists {
    if (Test-Path $buildDir) {
        Remove-Item $buildDir -Recurse -Force
    }
}

function Ensure-Vcpkg {
    if (-not (Test-Path $vcpkgExe)) {
        $bootstrap = Join-Path $vcpkgRoot "bootstrap-vcpkg.bat"
        if (-not (Test-Path $bootstrap)) {
            throw "vcpkg.exe and bootstrap-vcpkg.bat were not found."
        }

        & $bootstrap -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to bootstrap vcpkg."
        }
    }

    if (-not (Test-Path $toolchain)) {
        throw "vcpkg toolchain file was not found: $toolchain"
    }
}

function Get-FirstExistingPath {
    param(
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Normalize-PathString {
    param(
        [string]$PathValue
    )

    if (-not $PathValue) {
        return $null
    }

    $expanded = [Environment]::ExpandEnvironmentVariables($PathValue).Replace('/', '\')
    return [IO.Path]::GetFullPath($expanded)
}

function Get-BuildConfiguration {
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl) {
        return @{
            Name = "msvc"
            CCompiler = $null
            CxxCompiler = $null
            RcCompiler = $null
            Triplet = "x64-windows"
            PathPrefix = $null
        }
    }

    $mingwBin = Get-FirstExistingPath @(
        "C:\msys64\mingw64\bin",
        "C:\msys64\ucrt64\bin",
        "C:\mingw64\bin"
    )

    if ($mingwBin) {
        $gcc = Join-Path $mingwBin "gcc.exe"
        $gxx = Join-Path $mingwBin "g++.exe"
        $windres = Join-Path $mingwBin "windres.exe"

        if ((Test-Path $gcc) -and (Test-Path $gxx)) {
            return @{
                Name = "mingw"
                CCompiler = (Resolve-Path $gcc).Path
                CxxCompiler = (Resolve-Path $gxx).Path
                RcCompiler = if (Test-Path $windres) { (Resolve-Path $windres).Path } else { $null }
                Triplet = "x64-mingw-static"
                PathPrefix = $mingwBin
            }
        }
    }

    throw @"
No supported C++ compiler was found.

Supported options:
  1. Visual Studio Build Tools / Visual Studio with C++
  2. MSYS2 MinGW-w64 at C:\msys64\mingw64\bin

This project was previously built with MinGW from C:\msys64\mingw64\bin.
"@
}

function Get-CachedValue {
    param(
        [string]$CacheFile,
        [string]$Key
    )

    if (-not (Test-Path $CacheFile)) {
        return $null
    }

    $line = Get-Content $CacheFile |
        Where-Object { $_ -like "${Key}:*" } |
        Select-Object -First 1

    if (-not $line) {
        return $null
    }

    $parts = $line -split "=", 2
    if ($parts.Length -ne 2) {
        return $null
    }

    return $parts[1]
}

function Test-CacheCompatible {
    param(
        [hashtable]$Config
    )

    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $true
    }

    $cachedToolchain = Get-CachedValue -CacheFile $cacheFile -Key "CMAKE_TOOLCHAIN_FILE"
    $cachedTriplet = Get-CachedValue -CacheFile $cacheFile -Key "VCPKG_TARGET_TRIPLET"
    $cachedCxxCompiler = Get-CachedValue -CacheFile $cacheFile -Key "CMAKE_CXX_COMPILER"

    $resolvedToolchain = Normalize-PathString ((Resolve-Path $toolchain).Path)
    $normalizedCachedToolchain = Normalize-PathString $cachedToolchain
    if ($normalizedCachedToolchain -and $normalizedCachedToolchain -ne $resolvedToolchain) {
        return $false
    }

    if ($cachedTriplet -and $cachedTriplet -ne $Config.Triplet) {
        return $false
    }

    if ($Config.CxxCompiler) {
        if (-not $cachedCxxCompiler) {
            return $false
        }

        try {
            $resolvedCached = Normalize-PathString ((Resolve-Path $cachedCxxCompiler).Path)
            $resolvedCurrent = Normalize-PathString ((Resolve-Path $Config.CxxCompiler).Path)
            if ($resolvedCached -ne $resolvedCurrent) {
                return $false
            }
        } catch {
            return $false
        }
    }

    return $true
}

function Copy-MinGWRuntimeDlls {
    param(
        [hashtable]$Config
    )

    if ($Config.Name -ne "mingw" -or -not $Config.PathPrefix) {
        return
    }

    $runtimeDlls = @(
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    )

    foreach ($dllName in $runtimeDlls) {
        $source = Join-Path $Config.PathPrefix $dllName
        if (Test-Path $source) {
            Copy-Item $source -Destination (Join-Path $buildDir $dllName) -Force
        }
    }
}

if ($Clean) {
    Remove-BuildDirectoryIfExists
}

Ensure-Vcpkg
$config = Get-BuildConfiguration

if (-not (Test-CacheCompatible -Config $config)) {
    Write-Host "Existing CMake cache is incompatible with the current compiler/toolchain. Removing build directory."
    Remove-BuildDirectoryIfExists
}

$cacheFile = Join-Path $buildDir "CMakeCache.txt"
$ninjaFile = Join-Path $buildDir "build.ninja"
$canReuseExistingConfigure = (Test-Path $cacheFile) -and (Test-Path $ninjaFile) -and -not $Reconfigure

$originalPath = $env:PATH
try {
    if ($config.PathPrefix) {
        $env:PATH = "$($config.PathPrefix);$env:PATH"
    }

    if (-not $canReuseExistingConfigure) {
        $configureArgs = @(
            "-S", $root,
            "-B", $buildDir,
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=$BuildType",
            "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
            "-DVCPKG_TARGET_TRIPLET=$($config.Triplet)"
        )

        if ($config.CCompiler) {
            $configureArgs += "-DCMAKE_C_COMPILER=$($config.CCompiler)"
        }

        if ($config.CxxCompiler) {
            $configureArgs += "-DCMAKE_CXX_COMPILER=$($config.CxxCompiler)"
        }

        if ($config.RcCompiler) {
            $configureArgs += "-DCMAKE_RC_COMPILER=$($config.RcCompiler)"
        }

        & cmake @configureArgs
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed."
        }
    }

    $buildArgs = @("--build", $buildDir, "--parallel")
    if ($Target) {
        $buildArgs += @("--target", $Target)
    }

    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed."
    }

    Copy-MinGWRuntimeDlls -Config $config
}
finally {
    $env:PATH = $originalPath
}
