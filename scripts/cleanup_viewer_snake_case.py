#!/usr/bin/env python3
"""Remove snake_case viewer duplicates superseded by PascalCase sources."""
from __future__ import annotations

import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
VIEWER = ROOT / "apps" / "viewer"

CMAKE_FILES = {
    "adapter/SceneAdapter.cpp",
    "adapter/DocumentService.cpp",
    "ecs/world.cpp",
    "ecs/systems.cpp",
    "commands/CommandRegistry.cpp",
    "commands/BuiltinCommands.cpp",
    "commands/CommandManager.cpp",
    "commands/DocumentHistory.cpp",
    "commands/picking.cpp",
    "commands/snap/SnapSettings.cpp",
    "commands/snap/accusnap.cpp",
    "commands/snap/SnapOverlay.cpp",
    "commands/CommandPalette.cpp",
    "commands/tools/CreateBoxTool.cpp",
    "commands/tools/CreateSphereTool.cpp",
    "commands/tools/CopyTool.cpp",
    "io/DxfExport.cpp",
    "document.cpp",
    "VulkanWindow.cpp",
    "VulkanRenderer.cpp",
    "render/VulkanBuffer.cpp",
    "render/VulkanRendererTextures.cpp",
    "render/VulkanPipeline.cpp",
    "render/MeshUploader.cpp",
    "render/VulkanDraw.cpp",
    "SelectRectOverlay.cpp",
    "app/ViewerApp.cpp",
    "assets/AssetCatalog.cpp",
    "MainWindow.cpp",
    "ui/ViewManager.cpp",
    "ui/CursorTip.cpp",
    "ui/ContextMenu.cpp",
    "ui/CommandsBridge.cpp",
    "ui/MainWindowMenus.cpp",
    "ui/PropertyDockSetup.cpp",
    "ui/WindowLifecycle.cpp",
    "ui/ViewCubeLayout.cpp",
    "ui/InputRouter.cpp",
    "ui/SnapSettingsDialog.cpp",
    "HomeWindow.cpp",
    "SplashScreen.cpp",
    "ViewCube.cpp",
    "ViewMdiSubwindow.cpp",
    "PropertyPanel.cpp",
    "i18n/LanguageManager.cpp",
    "main.cpp",
    "tests/TestSceneAdapter.cpp",
    "tests/TestDocumentService.cpp",
    "tests/TestSnapOverlay.cpp",
}

CMAKE_CANONICAL = {p.replace("\\", "/") for p in CMAKE_FILES}


def snake_to_pascal(stem: str) -> str:
    return "".join(part.capitalize() for part in stem.split("_"))


def rel_posix(path: pathlib.Path) -> str:
    return path.relative_to(VIEWER).as_posix()


def counterpart_paths(path: pathlib.Path) -> list[pathlib.Path]:
    rel = path.relative_to(VIEWER)
    stem = path.stem
    suffix = path.suffix.lower()
    parent = VIEWER / rel.parent

    if suffix == ".hpp":
        targets = [parent / f"{snake_to_pascal(stem)}.h"]
    elif suffix in {".cpp", ".h"}:
        targets = [parent / f"{snake_to_pascal(stem)}{suffix}"]
    else:
        return []

    return [t for t in targets if t != path]


def is_legacy_snake(path: pathlib.Path) -> bool:
    stem = path.stem
    if path.suffix.lower() not in {".cpp", ".h", ".hpp"}:
        return False
    if stem in {"main", "document"}:
        return False
    return bool(re.search(r"_[a-z]", stem)) and stem[0].islower()


def hpp_counterpart(path: pathlib.Path) -> pathlib.Path | None:
    stem = path.stem
    parent = path.parent
    if "_" in stem:
        target = parent / f"{snake_to_pascal(stem)}.h"
    else:
        target = parent / f"{stem[:1].upper()}{stem[1:]}.h"
    return target if target != path else None


def main() -> None:
    removed: list[str] = []

    for path in sorted(VIEWER.rglob("*")):
        if not path.is_file():
            continue
        suffix = path.suffix.lower()
        if suffix not in {".cpp", ".h", ".hpp"}:
            continue

        rel = rel_posix(path)
        if rel in CMAKE_CANONICAL:
            continue

        counterparts: list[pathlib.Path] = []
        if suffix == ".hpp":
            alt = hpp_counterpart(path)
            if alt is not None:
                counterparts.append(alt)
        elif is_legacy_snake(path):
            counterparts.extend(counterpart_paths(path))

        if not counterparts or not any(c.exists() for c in counterparts):
            continue

        path.unlink()
        removed.append(rel)

    print(f"Removed {len(removed)} legacy snake_case / .hpp duplicates:")
    for rel in removed:
        print(f"  - {rel}")


if __name__ == "__main__":
    main()
