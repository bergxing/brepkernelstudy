# Task 3 Report: Viewer Snap Settings / Session and AccuSnap Resolve

## Status

DONE_WITH_CONCERNS

Task 3 is implemented in commit `286706c`. The viewer now owns default snap
settings and per-tool session state, injects both through `CommandContext`, and
provides a single `AccuSnap::resolve` path that ranks B-Rep snap candidates
inside the pixel aperture before falling back to the `y = 0` workplane.

## Implementation

- Added `SnapSettings` defaults for Endpoint, Midpoint, Center, Intersection,
  Perpendicular, and Nearest. Grid remains disabled by default.
- Added `SnapSession` with `last_point`, hold override, and the reserved
  AccuDraw origin/axes/dynamic-input fields.
- Added `PickResult`, `snap_kind_priority`, `pick_best_candidate`, and
  `AccuSnap::resolve`.
- Resolve uses the active view camera or world main camera, computes the screen
  ray and workplane proximity hint, gathers all B-Rep bodies from the document
  main part, and calls `query_snap_candidates`.
- Grid and Workplane bits are stripped before the kernel call.
- Candidate ranking follows pixel distance, kind priority, then ray depth.
- `MainWindow` owns the settings/session and wires stable pointers into every
  generated command context.
- Added the new sources to `viewer_runtime`.

## Verification

Command:

```powershell
$env:PATH = "C:/Qt6/Tools/mingw1310_64/bin;C:/Qt6/6.8.3/mingw_64/bin;E:/brepkernelstudy/cmake-build-mingw-debug/bin;" + $env:PATH
cmake --build E:/brepkernelstudy/cmake-build-mingw-debug --target brep_viewer -j 8
```

Result: exit code 0. `viewer_runtime`, `viewer_ui`, and `brep_viewer.exe`
compiled and linked successfully. IDE diagnostics reported no linter errors in
the changed files.

## Tests and Concerns

- No automated viewer ranking test was added. The existing viewer test target
  links only `viewer_adapter`; adding ranking coverage would require expanding
  it to the Qt/Vulkan-linked `viewer_runtime`. The ranking function is kept
  explicit and externally callable for a later small test target.
- Grid resolution is intentionally stubbed off for this task, as allowed by the
  brief; `grid_enabled` and `grid_spacing` are reserved for Task 6.
- Tools are intentionally not migrated to `AccuSnap::resolve` yet (Task 4).
- No settings dialog or persistence was added (Task 6).
- Manual interactive snap smoke testing was not performed because tools do not
  consume the new resolver until Task 4.
- Existing untracked SDD metadata, build outputs, and `gpp_err*.txt` files were
  excluded from the implementation commit.

## Review Fixes

- Candidate ranking now requires a valid pick ray and rejects candidates with
  negative ray depth, preventing orthographic and perspective picks behind the
  camera.
- `PickResult` now defaults to `SnapKind::None`; `SnapKind::Workplane` is set
  only after a successful workplane intersection.
- Focused `brep_viewer` rebuild completed with exit code 0. `viewer_runtime`,
  `viewer_ui`, and `brep_viewer.exe` compiled and linked successfully. The
  deployment step retained its existing warning that `dxcompiler.dll` and
  `dxil.dll` were not found.
