# Bootstrap in-repo third-party deps for brep-kernel.
#
# Layout:
#   third_party/eigen            git submodule
#   third_party/spdlog           git submodule
#   third_party/Vulkan-Headers   git submodule
#   third_party/volk             git submodule
#   third_party/vcpkg            git submodule (provides Qt via manifest)
#   vcpkg.json                   Qt6 qtbase[vulkan] + shaderc
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

$Vcpkg = Join-Path $Root "third_party\vcpkg\vcpkg.exe"
if (-not (Test-Path $Vcpkg)) {
  Write-Host "==> bootstrap-vcpkg.bat"
  & (Join-Path $Root "third_party\vcpkg\bootstrap-vcpkg.bat") -disableMetrics
}

Write-Host "==> vcpkg install (manifest: qtbase + shaderc). This may take a long time."
& $Vcpkg install --triplet x64-windows

$Toolchain = Join-Path $Root "third_party\vcpkg\scripts\buildsystems\vcpkg.cmake"
Write-Host @"

Done.

Configure / build viewer:

  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$Toolchain"

  cmake --build build --config Release --target brep_viewer
  .\build\Release\brep_viewer.exe

Notes:
  - Vulkan headers/loader API come from third_party/Vulkan-Headers + volk
  - Runtime still needs a Vulkan-capable GPU driver (vulkan-1.dll)
  - Qt comes from the in-repo vcpkg submodule (not a Qt source submodule)

"@
