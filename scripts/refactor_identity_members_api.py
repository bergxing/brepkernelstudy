#!/usr/bin/env python3
"""Update IObject / Named / ParameterId / FeatureId member call sites."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {
    "third_party",
    "cmake-build-mingw-debug",
    "build-kernel",
    "build-mingw",
    ".git",
    "build",
    "install-kernel",
}

# Order matters: longer / more specific patterns first.
REPLACEMENTS = [
    ("obj->feature_guid", "obj->FeatureGuid"),
    ("obj.feature_guid", "obj.FeatureGuid"),
    ("obj->body_guid", "obj->BodyGuid"),
    ("obj.body_guid", "obj.BodyGuid"),
    ("obj->type_name", "obj->TypeName"),
    ("obj.type_name", "obj.TypeName"),
    ("fref->feature_guid", "fref->FeatureGuid"),
    (".feature_guid", ".FeatureGuid"),
    ("param.id", "param.Id"),
    ("body.id", "body.Id"),
    ("c.id", "c.Id"),
    ("body.name", "body.Name"),
    ("face.name", "face.Name"),
    ("e.name", "e.Name"),
    ("a.name", "a.Name"),
    ("b.name", "b.Name"),
    ("occ.name", "occ.Name"),
    ("doc.name", "doc.Name"),
    ("part.name", "part.Name"),
    ("p.name", "p.Name"),
    ("d.name", "d.Name"),
    ("fd.name", "fd.Name"),
    ("material_.name", "material_.Name"),
    ("out.name", "out.Name"),
    (".id.guid", ".Id.Guid"),
    (".Id().guid", ".Id().Guid"),
    ("fid.guid", "fid.Guid"),
    ("param->id", "param->Id"),
    ("p.id", "p.Id"),
    ("id.guid", "id.Guid"),
    ("->guid", "->Guid"),
    ("spec.name", "spec.Name"),
    ("->name", "->Name"),
    (".name =", ".Name ="),
    ("object.name", "object.Name"),
    ("object.guid", "object.Guid"),
    ("ref.name", "ref.Name"),
    ("part.guid", "part.Guid"),
    ("->id", "->Id"),
    (".guid", ".Guid"),
]

SKIP_PATH_PARTS = {
    ("apps", "viewer", "i18n"),
}


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(skip in path.parts for skip in SKIP_DIRS):
        return False
    if "apps" in path.parts and "viewer" not in path.parts:
        return False
    for skip in SKIP_PATH_PARTS:
        if skip == tuple(path.parts[i : i + len(skip)] for i in range(len(path.parts))):
            pass
    parts = path.parts
    for i in range(len(parts) - 2):
        if parts[i : i + 3] == ("apps", "viewer", "i18n"):
            return False
    return True


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in REPLACEMENTS:
        updated = updated.replace(old, new)
    if "ecs" in path.parts:
        updated = updated.replace("registry_.Create()", "registry_.create()")
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
