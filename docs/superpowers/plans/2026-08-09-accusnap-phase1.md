# AccuSnap Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship CAD-like AccuSnap (B-Rep object snap + grid/workplane) for viewer modeling tools per `docs/superpowers/specs/2026-08-09-accusnap-design.md`, with AccuDraw hooks reserved only.

**Architecture:** Kernel `query_snap_candidates` emits world-space B-Rep candidates; Viewer `AccuSnap::resolve` applies aperture, priority, grid, settings/keys, markers/tip; tools consume a single `PickResult`.

**Tech Stack:** C++20, CMake/Ninja (MinGW), GoogleTest, Qt 6 Widgets, existing `Picking.h` / `CommandContext` / `ITool`.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-08-09-accusnap-design.md` is authoritative.
- Snap truth = B-Rep topology, never tessellation vertices.
- Kernel must not score by pixels, grid, or UI priority.
- Tools must call one resolve entry; do not duplicate snap in VulkanWindow vs MainWindow.
- AccuDraw UI is out of Phase 1; keep reserved fields on `SnapSession` only.
- i18n: user-visible strings via `tr()` / `QCoreApplication::translate` + `apps/viewer/i18n/xcad_zh_CN.ts`.
- Build/verify with MinGW preset: `cmake-build-mingw-debug`, PATH includes Qt MinGW bins.
- Do not commit `gpp_err*.txt` or build-tree junk.

## File map

| File | Responsibility |
|------|----------------|
| `kernel/include/brep/snap/SnapTypes.h` | `SnapKind`, `SnapCandidate`, `SnapQuery` |
| `kernel/include/brep/snap/SnapQuery.h` | `query_snap_candidates` declaration |
| `kernel/include/api/Snap.h` | Graded public include |
| `kernel/src/snap/SnapQuery.cpp` | Candidate generation |
| `cmake/BrepCore.cmake` | Compile `SnapQuery.cpp` into `brep_core` |
| `tests/kernel/TestSnapQuery.cpp` | Kernel gtests |
| `tests/CMakeLists.txt` | Register `brep_test_snap` |
| `apps/viewer/commands/snap/SnapTypes.h` | Viewer `PickResult`, settings/session wrappers (or include kernel types) |
| `apps/viewer/commands/snap/SnapSettings.h/.cpp` | Settings + QSettings load/save |
| `apps/viewer/commands/snap/Accusnap.h/.cpp` | `resolve` + scoring + workplane/grid |
| `apps/viewer/commands/snap/SnapOverlay.h/.cpp` | Marker `EdgeMesh` glyphs |
| `apps/viewer/commands/CommandTypes.h` | Inject settings/session (+ optional tip/overlay callbacks) |
| `apps/viewer/ui/CommandsBridge.cpp` | Wire ctx fields from MainWindow |
| `apps/viewer/commands/tools/create_*.cpp`, `CopyTool.cpp` | Use AccuSnap |
| `apps/viewer/ui/CursorTip.cpp` (+ callers) | Show snap kind |
| `apps/viewer/ui/MainWindowMenus.cpp` | Toolbar toggle + Snap Settings… |
| `apps/viewer/ui/SnapSettingsDialog.h/.cpp` | Settings dialog |
| `apps/viewer/CMakeLists.txt` | New sources |
| `apps/viewer/i18n/xcad_zh_CN.ts` | Translations |

---

### Task 1: Kernel snap types + Endpoint/Midpoint/Center

**Files:**
- Create: `kernel/include/brep/snap/SnapTypes.h`
- Create: `kernel/include/brep/snap/SnapQuery.h`
- Create: `kernel/include/api/Snap.h`
- Create: `kernel/src/snap/SnapQuery.cpp`
- Modify: `cmake/BrepCore.cmake`
- Create: `tests/kernel/TestSnapQuery.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Body`, `Edge`, `Vertex`, `Face`, `Loop`, `Curve::eval`, `make_box`, `make_sphere` / `Part::add_sphere`
- Produces:
  - `enum class SnapKind : std::uint32_t` with bit values
  - `struct SnapCandidate { SnapKind kind; Point3d point; Guid body_guid; }`
  - `struct SnapQuery { std::uint32_t kinds; std::optional<Point3d> near_point; std::optional<Point3d> reference_point; double tolerance{1e-7}; }`
  - `std::vector<SnapCandidate> query_snap_candidates(std::span<Body* const> bodies, const SnapQuery& query);`

- [ ] **Step 1: Write failing tests**

Create `tests/kernel/TestSnapQuery.cpp`:

```cpp
#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Snap.h"

#include <gtest/Gtest.h>

#include <algorithm>
#include <cmath>

namespace brep {
namespace {

bool has_kind_near(const std::vector<SnapCandidate>& cands, SnapKind kind,
                   const Point3d& p, double eps = 1e-6) {
  return std::any_of(cands.begin(), cands.end(), [&](const SnapCandidate& c) {
    return c.kind == kind && (c.point - p).norm() < eps;
  });
}

TEST(SnapQuery, BoxEndpointsAndMidpoints) {
  Model model;
  Body* body = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 1, 3}, .name = "b"});
  ASSERT_NE(body, nullptr);

  SnapQuery q;
  q.kinds = static_cast<std::uint32_t>(SnapKind::Endpoint) |
            static_cast<std::uint32_t>(SnapKind::Midpoint);
  auto cands = query_snap_candidates(std::span<Body* const>{&body, 1}, q);

  EXPECT_TRUE(has_kind_near(cands, SnapKind::Endpoint, {0, 0, 0}));
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Endpoint, {2, 1, 3}));
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Midpoint, {1, 0, 0}));  // bottom edge mid
}

TEST(SnapQuery, SphereCenter) {
  Model model;
  Body* body = make_sphere(model, SphereSpec{.center = {1, 2, 3}, .radius = 2.0, .name = "s"});
  ASSERT_NE(body, nullptr);

  SnapQuery q;
  q.kinds = static_cast<std::uint32_t>(SnapKind::Center);
  auto cands = query_snap_candidates(std::span<Body* const>{&body, 1}, q);
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Center, {1, 2, 3}, 1e-3));
}

}  // namespace
}  // namespace brep
```

Register in `tests/CMakeLists.txt` like `brep_test_math`.

- [ ] **Step 2: Run tests — expect compile/link fail**

```powershell
$env:PATH = "C:/Qt6/Tools/mingw1310_64/bin;C:/Qt6/6.8.3/mingw_64/bin;" + $env:PATH
cmake --build E:/brepkernelstudy/cmake-build-mingw-debug --target brep_test_snap -j 8
```

Expected: FAIL (missing headers / target).

- [ ] **Step 3: Implement types + query (Endpoint/Midpoint/Center only)**

`SnapTypes.h` — bitflag enum:

```cpp
enum class SnapKind : std::uint32_t {
  None = 0,
  Endpoint = 1u << 0,
  Midpoint = 1u << 1,
  Center = 1u << 2,
  Intersection = 1u << 3,
  Perpendicular = 1u << 4,
  Nearest = 1u << 5,
  Grid = 1u << 6,       // viewer-only; kernel ignores
  Workplane = 1u << 7,  // viewer-only; kernel ignores
};
```

`SnapQuery.cpp` logic:
- Walk `body->shells` → faces → loops → coedges → unique `Edge*` set.
- Endpoint: emit `v0`/`v1` positions; dedupe points within `tolerance`.
- Midpoint: `edge->curve->eval(0.5*(t0+t1))` if curve else midpoint of endpoints.
- Center:
  - Planar face: average of outer-loop vertex positions (deduped).
  - Sphere heuristic: if face count high and vertices share a common radius to centroid within relative tolerance, emit that centroid once per body (covers `make_sphere`).

Add `SnapQuery.cpp` to `BrepCore.cmake`.  
`api/Snap.h` includes `brep/snap/SnapTypes.h` + `brep/snap/SnapQuery.h`.

- [ ] **Step 4: Run tests — expect PASS**

```powershell
cmake --build E:/brepkernelstudy/cmake-build-mingw-debug --target brep_test_snap -j 8
ctest --test-dir E:/brepkernelstudy/cmake-build-mingw-debug -R brep_test_snap --output-on-failure
```

Expected: all SnapQuery tests PASS.

- [ ] **Step 5: Commit**

```powershell
git add kernel/include/brep/snap kernel/include/api/Snap.h kernel/src/snap cmake/BrepCore.cmake tests/kernel/TestSnapQuery.cpp tests/CMakeLists.txt
git commit -m "Add kernel SnapQuery for endpoint, midpoint, and center candidates."
```

---

### Task 2: Kernel Intersection / Perpendicular / Nearest

**Files:**
- Modify: `kernel/src/snap/SnapQuery.cpp`
- Modify: `tests/kernel/TestSnapQuery.cpp`

**Interfaces:**
- Consumes: Task 1 API; `SnapQuery::reference_point`
- Produces: same `query_snap_candidates`, now honoring Intersection / Perpendicular / Nearest for **line segments only**

- [ ] **Step 1: Add failing tests**

```cpp
TEST(SnapQuery, LineLineIntersection) {
  // Two boxes or two explicit edges that cross in XY — simplest: one box and
  // assert known mid-edge intersections are not required; build two coplanar
  // crossing segments via a small helper body OR use two boxes whose edges
  // share a known crossing after translation.
  // Prefer: Model with two LineCurve edges paired into wire bodies if available;
  // else use make_box at (0,0,0)-(1,1,1) and (0.5,-0.5,0)-(1.5,0.5,1) and
  // look for Intersection near (0.5,0.5,0) or document the expected point from edges.
}

TEST(SnapQuery, PerpendicularFoot) {
  Model model;
  Body* body = make_box(model, BoxSpec{.min={0,0,0}, .max={2,0,0} /* invalid */});
  // Use normal box; reference_point = {1, 2, 0}; expect foot on bottom edge ~ {1,0,0}
  Body* b = make_box(model, BoxSpec{.min={0,0,0}, .max={2,1,1}, .name="b"});
  SnapQuery q;
  q.kinds = static_cast<std::uint32_t>(SnapKind::Perpendicular);
  q.reference_point = Point3d{1, 2, 0};
  auto cands = query_snap_candidates(std::span<Body* const>{&b, 1}, q);
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Perpendicular, {1, 0, 0}));
}

TEST(SnapQuery, NearestClamped) {
  Model model;
  Body* b = make_box(model, BoxSpec{.min={0,0,0}, .max={2,1,1}, .name="b"});
  SnapQuery q;
  q.kinds = static_cast<std::uint32_t>(SnapKind::Nearest);
  q.near_point = Point3d{1, -0.5, 0};
  auto cands = query_snap_candidates(std::span<Body* const>{&b, 1}, q);
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Nearest, {1, 0, 0}));
}
```

Implement `LineLineIntersection` with two boxes whose edges actually intersect, or create edges via `Model::make_line` + wire body if the codebase allows; if wire bodies are awkward, add a test-only path using two boxes and assert any emitted Intersection lies on both supporting lines within tolerance.

- [ ] **Step 2: Run — expect FAIL on new asserts**

- [ ] **Step 3: Implement line helpers in `SnapQuery.cpp`**

- `segment_closest_points(a0,a1,b0,b1)` → points + distance; if distance ≤ tolerance emit Intersection.
- Perpendicular: for each line edge, foot from `reference_point` clamped to segment.
- Nearest: for each line edge, foot from `near_point` (required) clamped to segment; emit Nearest.
- Skip non-`LineCurve` edges for these three kinds in Phase 1.

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit**

```powershell
git commit -m "Extend SnapQuery with line intersection, perpendicular, and nearest."
```

---

### Task 3: Viewer SnapSettings / SnapSession + AccuSnap::resolve (1a)

**Files:**
- Create: `apps/viewer/commands/snap/SnapSettings.h`
- Create: `apps/viewer/commands/snap/SnapSettings.cpp`
- Create: `apps/viewer/commands/snap/Accusnap.h`
- Create: `apps/viewer/commands/snap/accusnap.cpp`
- Modify: `apps/viewer/commands/CommandTypes.h`
- Modify: `apps/viewer/ui/CommandsBridge.cpp` (`make_command_context`)
- Modify: `apps/viewer/MainWindow.h` (own settings/session members)
- Modify: `apps/viewer/CMakeLists.txt` (`viewer_runtime` sources)

**Interfaces:**
- Consumes: `query_snap_candidates`, `screen_to_ray`, `world_to_screen`, `intersect_plane_y`
- Produces:
  - `struct PickResult { Point3d point; SnapKind kind; bool snapped; std::optional<SnapCandidate> candidate; }`
  - `struct SnapSettings { bool enabled{true}; std::uint32_t kinds; int aperture_px{12}; bool grid_enabled{false}; double grid_spacing{1.0}; }`
  - `struct SnapSession { std::optional<Point3d> last_point; std::optional<Point3d> origin; std::optional<Vector3d> axis_x; std::optional<Vector3d> axis_y; int dynamic_input_mode{0}; std::optional<SnapKind> hold_override; }`
  - `PickResult AccuSnap::resolve(CommandContext& ctx, float sx, float sy);`

- [ ] **Step 1: Add a small headless ranking test (optional but preferred)**

If wiring gtest into viewer is heavy, put pure functions in `accusnap.cpp`:

```cpp
// testable without Qt window:
int snap_kind_priority(SnapKind k);
std::optional<SnapCandidate> pick_best_candidate(
    const std::vector<SnapCandidate>& cands,
    const Camera& cam, int w, int h, float sx, float sy,
    int aperture_px, std::optional<SnapKind> override_kind);
```

Add `tests/viewer/test_accusnap_rank.cpp` **only if** viewer test target already exists / easy; otherwise cover ranking with a free function test in kernel-adjacent unit later and manually verify. Prefer: keep ranking functions in anonymous namespace with declaration in header for a tiny `brep_test_accusnap` under `apps/viewer/tests` if present.

Check `apps/viewer/tests` — if adapter tests exist, add ranking test there linked to `viewer_runtime` if feasible; else skip automated viewer test and rely on manual smoke + kernel tests.

- [ ] **Step 2: Implement settings defaults**

Default `kinds` = Endpoint|Midpoint|Center|Intersection|Perpendicular|Nearest (Grid off until Task 6).

- [ ] **Step 3: Implement `AccuSnap::resolve`**

```text
if !settings || !settings.enabled → workplane only
camera from ctx.view_camera ?? world->main_camera()
ray = screen_to_ray
bodies = all Body* from document main part (or coarse-filter via mesh hit body + neighbors)
q.kinds = settings.kinds filtered by hold_override; strip Grid|Workplane bits before kernel
q.reference_point = session->last_point
q.near_point = optional hit of ray with y=0 as proximity hint
cands = query_snap_candidates(...)
project each to screen; discard outside aperture_px
score; pick winner
else workplane intersect_plane_y(..., 0)
return PickResult
```

- [ ] **Step 4: Wire `CommandContext::snap_settings` / `snap_session` pointers from MainWindow.**

- [ ] **Step 5: Build `viewer_runtime` / `brep_viewer`**

```powershell
cmake --build E:/brepkernelstudy/cmake-build-mingw-debug --target brep_viewer -j 8
```

Expected: success.

- [ ] **Step 6: Commit**

```powershell
git commit -m "Add viewer AccuSnap resolve with settings session and workplane fallback."
```

---

### Task 4: Migrate sphere / box base / copy tools

**Files:**
- Modify: `apps/viewer/commands/tools/CreateSphereTool.cpp`
- Modify: `apps/viewer/commands/tools/CreateBoxTool.cpp`
- Modify: `apps/viewer/commands/tools/CopyTool.cpp`

**Interfaces:**
- Consumes: `AccuSnap::resolve`
- Produces: tools update `session->last_point` after successful press picks

- [ ] **Step 1: Replace `CreateSphereTool::pick_point` with AccuSnap resolve; keep failure messaging.**

- [ ] **Step 2: Replace `CreateBoxTool::pick_ground` for steps 0–1; leave `pick_height` as vertical-plane tool constraint.**

- [ ] **Step 3: Replace `CopyTool::pick_ground`.**

- [ ] **Step 4: On each successful modeling pick, set `ctx.snap_session->last_point = result.point`.**

- [ ] **Step 5: Clear `last_point` in each tool `on_start`.**

- [ ] **Step 6: Build + manual smoke checklist (box on corner, sphere on endpoint).**

- [ ] **Step 7: Commit**

```powershell
git commit -m "Route box, sphere, and copy picks through AccuSnap."
```

---

### Task 5: Snap marker overlay + cursor tip

**Files:**
- Create: `apps/viewer/commands/snap/SnapOverlay.h/.cpp`
- Modify: `apps/viewer/VulkanWindow.h/.cpp` and/or renderer — add `set_snap_overlay(EdgeMesh)` drawn after tool preview
- Modify: `apps/viewer/ui/CursorTip.cpp` + `InputRouter.cpp` / `CommandsBridge.cpp` to pass last snap label
- Modify: `apps/viewer/i18n/xcad_zh_CN.ts`

**Interfaces:**
- Consumes: winning `PickResult`
- Produces: `EdgeMesh make_snap_marker(SnapKind, Point3d)`; tip text `snap_kind_name(kind)`

- [ ] **Step 1: Implement glyph meshes (small world-size or screen-constant via camera distance scale).**

- [ ] **Step 2: Add renderer/window snap overlay channel (do not clear tool preview).**

- [ ] **Step 3: AccuSnap::resolve (or tools after resolve) update overlay + store last kind for tip.**

- [ ] **Step 4: Cursor tip shows `prompt` + snap name when `snapped`.**

- [ ] **Step 5: Add zh_CN translations for snap kind names.**

- [ ] **Step 6: Build + visual smoke.**

- [ ] **Step 7: Commit**

```powershell
git commit -m "Show AccuSnap markers and localized cursor tip labels."
```

---

### Task 6: Settings UI, override keys, Grid + QSettings (1b/1c)

**Files:**
- Create: `apps/viewer/ui/SnapSettingsDialog.h/.cpp`
- Modify: `apps/viewer/ui/MainWindowMenus.cpp` (toggle + dialog action + retranslate)
- Modify: `apps/viewer/commands/snap/SnapSettings.cpp` (QSettings)
- Modify: `apps/viewer/ui/InputRouter.cpp` and/or `command_manager` key path for hold overrides + F3
- Modify: `apps/viewer/commands/snap/accusnap.cpp` (grid quantization)
- Modify: `apps/viewer/CMakeLists.txt` (`viewer_ui` sources)
- Modify: `apps/viewer/i18n/xcad_zh_CN.ts`

**Interfaces:**
- Consumes: `SnapSettings`
- Produces: dialog edits settings; keys set `session.hold_override`; Grid candidate from workplane hit quantized to spacing

- [ ] **Step 1: Implement Grid in resolve** — if Grid enabled (and master on), from workplane hit round X/Z (or UV) to `grid_spacing`; include as candidate with kind Grid; score with other candidates.

- [ ] **Step 2: QSettings keys under `snap/` group: enabled, kinds, aperture_px, grid_enabled, grid_spacing.**

- [ ] **Step 3: Snap Settings dialog with checkboxes + aperture spin + grid spacing + disabled AccuDraw row.**

- [ ] **Step 4: Toolbar master toggle bound to `settings.enabled`.**

- [ ] **Step 5: While tool active, track key press/release for E/M/C/I/P/G → `hold_override`; F3 toggles master.**

- [ ] **Step 6: i18n for dialog/toolbar.**

- [ ] **Step 7: Build + manual smoke (grid, hold E, master off).**

- [ ] **Step 8: Commit**

```powershell
git commit -m "Add AccuSnap settings UI, override keys, grid snap, and QSettings."
```

---

### Task 7: Phase 1 verification gate

**Files:** none new (checklist)

- [ ] **Step 1: Run kernel tests**

```powershell
$env:PATH = "C:/Qt6/Tools/mingw1310_64/bin;C:/Qt6/6.8.3/mingw_64/bin;E:/brepkernelstudy/cmake-build-mingw-debug/bin;" + $env:PATH
ctest --test-dir E:/brepkernelstudy/cmake-build-mingw-debug -R "brep_test_math|brep_test_snap|box_demo|smoke|parametric_smoke|xl_roundtrip" --output-on-failure
```

Expected: listed tests PASS (ignore pre-existing `*_NOT_BUILT` placeholders if still present).

- [ ] **Step 2: Manual smoke against success criteria in the spec**

- [ ] **Step 3: Update spec status line to `Accepted / Implemented Phase 1` if all pass (optional commit).**

- [ ] **Step 4: Final commit only if status/doc tweaks remain.**

---

## Spec coverage checklist

| Spec item | Task |
|-----------|------|
| Kernel SnapQuery B-Rep candidates | 1–2 |
| Viewer AccuSnap resolve + workplane | 3 |
| Tool migration box/sphere/copy | 4 |
| Marker + tip + i18n | 5 |
| Settings, keys, grid, QSettings | 6 |
| AccuDraw reserved fields | 3 (`SnapSession`) |
| AccuDraw UI not shipped | 6 (disabled row only) |
| Automated kernel tests | 1–2, 7 |
| Box height AccuSnap deferred | 4 (explicit) |

## Placeholder / consistency review

- No TBD steps; Intersection test notes concrete strategy.
- Types: `SnapKind`, `SnapCandidate`, `SnapQuery`, `PickResult`, `SnapSettings`, `SnapSession`, `AccuSnap::resolve` used consistently.
- Grid/Workplane bits exist on enum but kernel ignores them.
