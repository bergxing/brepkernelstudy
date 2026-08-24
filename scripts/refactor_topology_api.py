#!/usr/bin/env python3
"""Update Topology / ObjectRegistry call sites after PascalCase refactor."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}

TOPOLOGY_METHODS = [
    ("for_each_coedge", "ForEachCoedge"),
    ("outer_loops", "OuterLoops"),
    ("inner_loops", "InnerLoops"),
    ("outer_loop", "OuterLoop"),
    ("outer_shell", "OuterShell"),
    ("face_count", "FaceCount"),
    ("is_closed", "IsClosed"),
    ("normal_at", "NormalAt"),
    ("param_at", "ParamAt"),
    ("->face()", "->GetFace()"),
    (".face()", ".GetFace()"),
]

LOOP_SIZE_VARS = (
    "loop",
    "outer",
    "inner",
    "lp",
    "bottom_outer",
    "trim_boundary",
    "l",
)

REGISTRY_METHODS = [
    ("ObjectRegistry::add", "ObjectRegistry::Add"),
    ("ObjectRegistry::remove", "ObjectRegistry::Remove"),
    ("ObjectRegistry::clear", "ObjectRegistry::Clear"),
    ("ObjectRegistry::find", "ObjectRegistry::Find"),
    ("ObjectRegistry::find_as", "ObjectRegistry::FindAs"),
    ("ObjectRegistry::size", "ObjectRegistry::Size"),
    ("Registry().find", "Registry().Find"),
    ("Registry().add", "Registry().Add"),
    ("Registry().remove", "Registry().Remove"),
    ("Registry().clear", "Registry().Clear"),
    ("m_registry.add", "m_registry.Add"),
    ("m_registry.remove", "m_registry.Remove"),
    ("m_registry.clear", "m_registry.Clear"),
    ("m_registry.find", "m_registry.Find"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def replace_topology_methods(text: str) -> str:
    for old, new in TOPOLOGY_METHODS:
        text = text.replace(old, new)
    text = re.sub(
        rf"\b({'|'.join(LOOP_SIZE_VARS)})(->|\.)size\(\)",
        r"\1\2CoedgeCount()",
        text,
    )
    text = text.replace("OuterLoop()->size()", "OuterLoop()->CoedgeCount()")
    text = re.sub(
        r"(\w+)->start\(",
        r"\1->Start(",
        text,
    )
    text = re.sub(
        r"(\w+)->end\(",
        r"\1->End(",
        text,
    )
    text = re.sub(
        r"(\w+)->from\(\)",
        r"\1->From()",
        text,
    )
    text = re.sub(
        r"(\w+)->to\(\)",
        r"\1->To()",
        text,
    )
    text = re.sub(
        r"(\w+)\.from\(\)",
        r"\1.From()",
        text,
    )
    text = re.sub(
        r"(\w+)\.to\(\)",
        r"\1.To()",
        text,
    )
    return text


def replace_vertex_position(text: str, *, include_viewer: bool) -> str:
    if not include_viewer and "apps" in str(text):
        return text

    def repl(match: re.Match[str]) -> str:
        prefix = match.group(1)
        if prefix in {"event", "e"}:
            return match.group(0)
        return f"{prefix}->Position()"

    text = re.sub(r"\]->position\(\)", "]->Position()", text)
    text = re.sub(r"(\w+)->position\(\)", repl, text)
    text = re.sub(r"\)->position\(\)", ")->Position()", text)
    return text


def replace_registry_methods(text: str) -> str:
    for old, new in REGISTRY_METHODS:
        text = text.replace(old, new)
    return text


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    updated = replace_topology_methods(updated)
    updated = replace_registry_methods(updated)
    rel = path.relative_to(ROOT)
    if rel.parts[0] in {"kernel", "tests", "examples"}:
        updated = replace_vertex_position(updated, include_viewer=True)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        return True
    return False


def main() -> None:
    changed = 0
    for path in ROOT.rglob("*"):
        if path.is_file() and should_process(path) and process_file(path):
            changed += 1
            print(path.relative_to(ROOT))
    print(f"Updated {changed} files.")


if __name__ == "__main__":
    main()
