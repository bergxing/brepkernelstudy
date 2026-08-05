# Bootstrap in-repo third-party deps for brep-kernel.
#
# Layout:
#   third_party/eigen            git submodule
#   third_party/spdlog           git submodule
#   third_party/Vulkan-Headers   git submodule
#   third_party/volk             git submodule
#
# Qt is a LOCAL install (not vendored). Default search root: C:\Qt6
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
$QtPrefix = $null
foreach ($kit in @("msvc2022_64", "msvc2019_64", "mingw_64")) {
  $candidates = Get-ChildItem -Path $QtRoot -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName $kit }
  foreach ($c in $candidates) {
    if (Test-Path (Join-Path $c "lib\cmake\Qt6\Qt6Config.cmake")) {
      $QtPrefix = $c
      break
    }
  }
  if ($QtPrefix) { break }
}

if (-not $QtPrefix) {
  Write-Host @"
WARNING: Qt6 not found under $QtRoot
Install Qt 6 (MSVC 64-bit) or set BREP_QT_ROOT / CMAKE_PREFIX_PATH.
"@
} else {
  Write-Host "==> Detected Qt prefix: $QtPrefix"
}

Write-Host @"

Done.

Configure / build viewer (local Qt):

  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DBREP_QT_ROOT="$QtRoot"

  cmake --build build --config Release --target brep_viewer
  .\build\Release\brep_viewer.exe

Or pin the kit explicitly:

  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="C:\Qt6\6.7.3\msvc2022_64"

Notes:
  - Vulkan headers come from third_party/Vulkan-Headers (+ volk)
  - Runtime still needs a Vulkan-capable GPU driver (vulkan-1.dll)
  - Qt is used from the local install under C:\Qt6 (not a project submodule)

"@
