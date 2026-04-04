param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "build"
$vcpkgRoot = Join-Path $root "vcpkg"
$vcpkgExe = Join-Path $vcpkgRoot "vcpkg.exe"
$toolchain = Join-Path $vcpkgRoot "scripts\\buildsystems\\vcpkg.cmake"

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item $buildDir -Recurse -Force
}

if (-not (Test-Path $vcpkgExe)) {
    $bootstrap = Join-Path $vcpkgRoot "bootstrap-vcpkg.bat"
    if (-not (Test-Path $bootstrap)) {
        throw "vcpkg.exe and bootstrap-vcpkg.bat were not found."
    }

    & $bootstrap
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to bootstrap vcpkg."
    }
}

if (-not (Test-Path $toolchain)) {
    throw "vcpkg toolchain file was not found: $toolchain"
}

$resolvedToolchain = (Resolve-Path $toolchain).Path
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if (Test-Path $cacheFile) {
    $cachedToolchainLine = Get-Content $cacheFile |
        Where-Object { $_ -like "CMAKE_TOOLCHAIN_FILE:FILEPATH=*" } |
        Select-Object -First 1

    if (-not $cachedToolchainLine) {
        Remove-Item $buildDir -Recurse -Force
    } else {
        $cachedToolchain = $cachedToolchainLine.Substring("CMAKE_TOOLCHAIN_FILE:FILEPATH=".Length)
        $resolvedCachedToolchain = Resolve-Path $cachedToolchain -ErrorAction SilentlyContinue

        if (-not $resolvedCachedToolchain -or $resolvedCachedToolchain.Path -ine $resolvedToolchain) {
            Remove-Item $buildDir -Recurse -Force
        }
    }
}

$toolchainArgument = ""
if (-not (Test-Path $cacheFile)) {
    $toolchainArgument = " -DCMAKE_TOOLCHAIN_FILE=`"$resolvedToolchain`""
}

$vcvars = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -Filter "vcvars64.bat" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1

if (-not $vcvars) {
    throw "vcvars64.bat was not found. Install Visual Studio Build Tools or Visual Studio with C++."
}

$command = @(
    "`"$($vcvars.FullName)`""
    "&&"
    "cmake -S `"$root`" -B `"$buildDir`" -G Ninja -DCMAKE_CXX_COMPILER=cl$toolchainArgument"
    "&&"
    "cmake --build `"$buildDir`" -j"
) -join " "

cmd /c $command
exit $LASTEXITCODE
