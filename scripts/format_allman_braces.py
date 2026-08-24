#!/usr/bin/env python3
"""DEPRECATED — do not use.

This script corrupted sources by splitting `const`, `override`, and block
comments (e.g. `/*name*/`) when reformatting braces.

Use instead:
  - `clang-format` via `python scripts/format_all.py` (when available)
  - `python scripts/repair_allman_corruption.py` to fix prior damage

Exits with code 2 if invoked.
"""

import sys


def main() -> int:
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
