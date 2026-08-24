#!/usr/bin/env python3
"""Enforce Phase 4 include boundaries.

Rules:
  1. apps/viewer/** must not #include "brep/..." or <brep/...>
     (kernel API must come only via "api/...")
  2. apps/viewer/** must not #include "brep/internal/..." (defense in depth)
  3. examples/** and tests/** (non-viewer) must not include brep/internal/
  4. kernel/include/api/*.h may only #include "brep/..." (or api/) — no reverse deps
  5. Outside kernel/{src,include} and cmake build trees: forbid brep/internal/

Exit 0 on success; non-zero with a printed report on failure.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s*([<"])([^>"]+)([>"])',
    re.MULTILINE,
)

SOURCE_SUFFIXES = {".hpp", ".h", ".hh", ".cpp", ".cc", ".cxx", ".inl"}


def iter_sources(base: Path) -> list[Path]:
    if not base.is_dir():
        return []
    out: list[Path] = []
    for p in base.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        # Skip generated / build artifacts if any sneak under the tree.
        parts = {x.lower() for x in p.parts}
        if "cmake-build-mingw-debug" in parts or "cmake-build" in str(p).lower():
            if "cmake-build" in parts or any(
                part.startswith("cmake-build") for part in p.parts
            ):
                continue
        out.append(p)
    return out


def includes_in(path: Path) -> list[tuple[int, str, str]]:
    """Return (line, include_path, delimiter) where delimiter is '\"' or '<'."""
    text = path.read_text(encoding="utf-8", errors="replace")
    found: list[tuple[int, str, str]] = []
    for i, line in enumerate(text.splitlines(), start=1):
        m = INCLUDE_RE.match(line)
        if not m:
            continue
        found.append((i, m.group(2).replace("\\", "/"), m.group(1)))
    return found


def rel(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT)).replace("\\", "/")
    except ValueError:
        return str(path)


def check_viewer(errors: list[str]) -> None:
    viewer = ROOT / "apps" / "viewer"
    for path in iter_sources(viewer):
        for line_no, inc, _delim in includes_in(path):
            if inc.startswith("brep/"):
                errors.append(
                    f"{rel(path)}:{line_no}: viewer must not include '{inc}' "
                     f"(use api/*.h)"
                )
            if "brep/internal/" in inc:
                errors.append(
                    f"{rel(path)}:{line_no}: forbidden internal header '{inc}'"
                )


def check_no_internal_outside_kernel(errors: list[str]) -> None:
    roots = [
        ROOT / "apps",
        ROOT / "examples",
        ROOT / "tests",
        ROOT / "kernel" / "include" / "api",
    ]
    for base in roots:
        for path in iter_sources(base):
            for line_no, inc, _delim in includes_in(path):
                if "brep/internal/" in inc:
                    errors.append(
                        f"{rel(path)}:{line_no}: '{inc}' is kernel-private "
                        f"(brep/internal is not a public API)"
                    )


def check_api_headers(errors: list[str]) -> None:
    api_dir = ROOT / "kernel" / "include" / "api"
    if not api_dir.is_dir():
        errors.append("missing kernel/include/api/")
        return
    for path in sorted(api_dir.glob("*.hpp")):
        for line_no, inc, delim in includes_in(path):
            if delim != '"':
                continue  # allow <system> / <Eigen/...>
            if inc.startswith("api/"):
                continue
            if inc.startswith("brep/"):
                if "brep/internal/" in inc:
                    errors.append(
                        f"{rel(path)}:{line_no}: api headers must not expose "
                        f"internal '{inc}'"
                    )
                continue
            errors.append(
                f"{rel(path)}:{line_no}: api header may only #include "
                f"\"brep/...\" or \"api/...\" (found '{inc}')"
            )


def check_internal_not_on_public_layout(errors: list[str]) -> None:
    """Public tree must not host brep/internal (lives under kernel/internal/)."""
    public_internal = ROOT / "kernel" / "include" / "brep" / "internal"
    if public_internal.exists():
        errors.append(
            "kernel/include/brep/internal/ must not exist — keep private "
            "headers under kernel/internal/ (PRIVATE include only)"
        )
    private_root = ROOT / "kernel" / "internal"
    if not private_root.is_dir():
        errors.append("missing kernel/internal/ (PRIVATE include root)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="repository root (default: parent of scripts/)",
    )
    args = parser.parse_args()
    root = (args.root or Path(__file__).resolve().parents[1]).resolve()

    # Bind module-level ROOT used by helpers.
    global ROOT
    ROOT = root

    errors: list[str] = []
    check_internal_not_on_public_layout(errors)
    check_viewer(errors)
    check_no_internal_outside_kernel(errors)
    check_api_headers(errors)

    if errors:
        print(f"include boundary check FAILED ({len(errors)} issue(s)):", file=sys.stderr)
        for e in errors:
            print(f"  {e}", file=sys.stderr)
        return 1

    print("include boundary check OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
