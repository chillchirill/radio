param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "build"

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item $buildDir -Recurse -Force
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
    "cmake -S `"$root`" -B `"$buildDir`" -G Ninja -DCMAKE_CXX_COMPILER=cl"
    "&&"
    "cmake --build `"$buildDir`" -j"
) -join " "

cmd /c $command
exit $LASTEXITCODE
