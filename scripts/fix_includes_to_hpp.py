#!/usr/bin/env python3
"""DEPRECATED — do not use.

This script pointed #include paths at legacy snake_case `.hpp` files. That is
the **opposite** of project style.

Canonical naming (see `.cursor/skills/google-cpp-style/project-overrides.md`):
  - Headers: **PascalCase.h**  (e.g. `Model.h`, `BooleanPipeline.h`)
  - Sources: **PascalCase.cpp** (e.g. `Builder.cpp`, `MainWindow.cpp`)

Use instead:
  - `python scripts/migrate_style.py`   — rename `.hpp` → `.h`, snake_case → PascalCase
  - `python scripts/fix_includes.py`    — fix #include paths to PascalCase `.h`

Exits with code 2 if invoked.
"""

from __future__ import annotations

import sys


def main() -> int:
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
