# Bootstrap in-repo third-party deps for brep-kernel.
#
# Layout:
#   third_party/eigen            git submodule
#   third_party/spdlog           git submodule
#   third_party/Vulkan-Headers   git submodule
#   third_party/volk             git submodule
#
# Qt is a LOCAL install (not vendored). Default search root: C:\Qt6
# Recommended IDE: CLion + MinGW (no Visual Studio required).
#
# Usage (from repo root):
#   powershell -ExecutionPolicy Bypass -File .\scripts\setup_third_party.ps1

$ErrorActionPreference = "Stop"
$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $Root

Write-Host "==> git submodule update --init --recursive"
git submodule update --init --recursive

Write-Host "==> Generate viewer SPIR-V (Python emitter, no glslc required)"
python (Join-Path $Root "scripts\gen_spv.py")

$QtRoot = if ($env:BREP_QT_ROOT) { $env:BREP_QT_ROOT } else { "C:\Qt6" }

# Prefer newer MinGW shipped with recent Qt installers (13.1, then 11.2, …).
$MingwBin = $null
foreach ($name in @("mingw1310_64", "mingw1120_64")) {
  $candidate = Join-Path $QtRoot "Tools\$name\bin"
  if (Test-Path (Join-Path $candidate "g++.exe")) {
    $MingwBin = $candidate
    break
  }
}
if (-not $MingwBin) {
  $MingwBin = Get-ChildItem -Path (Join-Path $QtRoot "Tools") -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "mingw*" -and $_.Name -notlike "llvm-*" } |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName "bin" } |
    Where-Object { Test-Path (Join-Path $_ "g++.exe") } |
    Select-Object -First 1
}

function Find-QtKit([string]$Kit) {
  Get-ChildItem -Path $QtRoot -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName $Kit } |
    Where-Object { Test-Path (Join-Path $_ "lib\cmake\Qt6\Qt6Config.cmake") } |
    Select-Object -First 1
}

$QtMingw = Find-QtKit "mingw_64"
$QtMsvc = Find-QtKit "msvc2022_64"
if (-not $QtMsvc) { $QtMsvc = Find-QtKit "msvc2019_64" }

if ($QtMingw) {
  Write-Host "==> Detected Qt MinGW prefix: $QtMingw"
} else {
  Write-Host "WARNING: Qt mingw_64 kit not found under $QtRoot"
}
if ($MingwBin) {
  Write-Host "==> Detected MinGW toolchain: $MingwBin"
} else {
  Write-Host "WARNING: MinGW g++ not found under $QtRoot\Tools"
}

Write-Host @"

Done.

=== CLion (recommended, no Visual Studio) ===

1. File → Open → this repo folder
2. CMake settings → select preset: clion-mingw
3. Build target: brep_viewer  (or box_demo / smoke)
4. Run. If Qt DLLs are missing, add to Run Configuration → Environment:

   PATH=$MingwBin;$QtMingw\bin;%PATH%

Command-line equivalent (CLion uses Ninja; or use MinGW Makefiles):

  cmake --preset mingw-makefiles
  cmake --build --preset mingw-makefiles --target brep_viewer
  .\build-mingw\apps\viewer\brep_viewer.exe

=== Optional: MSVC (only if you already have it) ===

  cmake -S . -B build -G Ninja -DBREP_QT_ROOT="$QtRoot" -DCMAKE_PREFIX_PATH="$QtMsvc"

Notes:
  - Vulkan headers come from third_party/Vulkan-Headers (+ volk)
  - Runtime still needs a Vulkan-capable GPU driver (vulkan-1.dll)
  - Qt kit must match the compiler (mingw_64 with g++, msvc*_64 with cl)

"@
