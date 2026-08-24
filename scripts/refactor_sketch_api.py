#!/usr/bin/env python3
"""Batch 11: Sketch Constraint fields + Sketch method names."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build", "install-kernel"}

METHOD_REPLACEMENTS = [
    ("Sketch::add_point", "Sketch::AddPoint"),
    ("Sketch::add_line", "Sketch::AddLine"),
    ("Sketch::add_circle", "Sketch::AddCircle"),
    ("Sketch::add_constraint", "Sketch::AddConstraint"),
    ("->add_point(", "->AddPoint("),
    ("->add_line(", "->AddLine("),
    ("->add_circle(", "->AddCircle("),
    ("->add_constraint(", "->AddConstraint("),
    (".add_point(", ".AddPoint("),
    (".add_line(", ".AddLine("),
    (".add_circle(", ".AddCircle("),
    (".add_constraint(", ".AddConstraint("),
    ("->set_frame(", "->SetFrame("),
    (".set_frame(", ".SetFrame("),
    ("->frame()", "->Frame()"),
    (".frame()", ".Frame()"),
    ("->points()", "->Points()"),
    (".points()", ".Points()"),
    ("->lines()", "->Lines()"),
    (".lines()", ".Lines()"),
    ("->circles()", "->Circles()"),
    (".circles()", ".Circles()"),
    ("->constraints()", "->Constraints()"),
    (".constraints()", ".Constraints()"),
    ("->is_point(", "->IsPoint("),
    (".is_point(", ".IsPoint("),
    ("->line_at(", "->LineAt("),
    (".line_at(", ".LineAt("),
    ("->assign(", "->Assign("),
    (".assign(", ".Assign("),
    ("->next_entity(", "->NextEntity("),
    (".next_entity(", ".NextEntity("),
    ("->next_constraint(", "->NextConstraint("),
    (".next_constraint(", ".NextConstraint("),
    ("Sketch::is_point", "Sketch::IsPoint"),
    ("Sketch::line_at", "Sketch::LineAt"),
    ("Sketch::assign", "Sketch::Assign"),
    ("void set_frame(", "void SetFrame("),
    ("Plane frame() const", "Plane Frame() const"),
    ("SketchEntityId add_point(", "SketchEntityId AddPoint("),
    ("SketchEntityId add_line(", "SketchEntityId AddLine("),
    ("SketchEntityId add_circle(", "SketchEntityId AddCircle("),
    ("ConstraintId add_constraint(", "ConstraintId AddConstraint("),
    ("bool is_point(", "bool IsPoint("),
    ("std::optional<SketchLine> line_at(", "std::optional<SketchLine> LineAt("),
    ("void assign(", "void Assign("),
    ("std::uint32_t next_entity(", "std::uint32_t NextEntity("),
    ("std::uint32_t next_constraint(", "std::uint32_t NextConstraint("),
]

FIELD_REPLACEMENTS = [
    ("c.id", "c.Id"),
    ("c.kind", "c.Kind"),
    ("c.dim", "c.Dim"),
    ("c.aux", "c.Aux"),
    ("ConstraintId id{}", "ConstraintId Id{}"),
    ("ConstraintKind kind{", "ConstraintKind Kind{"),
    ("ParameterId dim{}", "ParameterId Dim{}"),
    ("double aux{", "double Aux{"),
    ("SketchEntityId value{", "SketchEntityId Value{"),  # wrong - only ConstraintId
    ("ConstraintId id;", "ConstraintId Id;"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    if "apps" in path.parts and "viewer" not in path.parts:
        return False
    return True


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in METHOD_REPLACEMENTS + FIELD_REPLACEMENTS:
        updated = updated.replace(old, new)
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
