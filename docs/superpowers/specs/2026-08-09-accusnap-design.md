# AccuSnap Design (Phase 1)

Date: 2026-08-09  
Status: Accepted / Implemented Phase 1 — automated verification passed; manual GUI smoke still recommended  
Branch context: `cursor/modern-cpp-brep-kernel`

## Goals

Deliver CAD/MicroStation-like **object snap (AccuSnap)** for the viewer:

- Snap sources are **B-Rep topology** (true geometry), not tessellation vertices.
- Global enable + per-kind toggles + **hold-to-override** keys.
- Grid + workplane fallback in the viewer.
- Tools share one resolve path (`PickResult`) instead of duplicated ground/mesh picks.
- **AccuDraw** (dynamic relative input) is explicitly **out of Phase 1**, with API hooks reserved.

## Non-goals (Phase 1)

- AccuDraw compass / polar / relative XY key-in UI
- Curve–curve intersection, tangency, parallel, extension, quadrant
- Assembly instance transform trees (bodies treated in world space as today)
- Persisting snap settings into XL documents
- Perfect large-scene spatial indexing (optional mesh coarse filter only)

## Decisions (locked)

| Topic | Choice |
|-------|--------|
| Architecture | Kernel `SnapQuery` + Viewer `AccuSnap` |
| Geometry source | B-Rep topology |
| Interaction depth | AccuSnap now; AccuDraw reserved |
| Controls | Master switch + kind checkboxes + hold overrides |
| Workplane (Phase 1) | Default `y = 0` (same as current ground) |

---

## §1 Architecture and data flow

### Layers

| Layer | Responsibility | Location |
|-------|----------------|----------|
| Kernel SnapQuery | Emit world-space candidates from B-Rep | `kernel` + public `api/snap.hpp` (name may vary) |
| Viewer AccuSnap | Aperture, priority, grid, settings/keys, markers/tip | `apps/viewer/commands/snap/` |
| Tools | Consume `PickResult`; no private ground/mesh pick duplication | box / sphere / copy (and future tools) |
| AccuDraw (Phase 2) | Relative origin/axes, ortho lock, key-in | Hooks only on `SnapSession` / `CommandContext` |

### Core types

```text
SnapKind:
  Endpoint | Midpoint | Center | Intersection |
  Perpendicular | Nearest | Grid | Workplane

SnapCandidate {
  kind
  point          // world
  body_guid
  // optional topology ids (vertex/edge/face) for debugging / Phase 2
  score_hint     // optional kernel hint; viewer may ignore
}

PickResult {
  point
  kind
  snapped        // false when pure workplane miss path without grid
  candidate?     // winning SnapCandidate when snapped
}

SnapSettings {
  enabled
  kinds          // bitmask / set
  aperture_px
  grid_enabled
  grid_spacing
  // key bindings optional in Phase 1 (can be hardcoded)
}

SnapSession {
  last_point?           // for Perpendicular / rubber-band
  // AccuDraw reserved:
  origin?
  axes?                 // e.g. x/y directions in workplane
  dynamic_input_mode?   // unused in Phase 1
}
```

### Resolve pipeline

```text
screen (sx, sy)
  → screen_to_ray(active camera, viewport)
  → AccuSnap::resolve(ctx, ray, settings, session)
       ├─ optional coarse body filter via existing mesh raycast
       ├─ Kernel: query_snap_candidates(bodies, query)
       ├─ Viewer: project candidates to screen; keep aperture hits
       ├─ Viewer: add Grid / Workplane candidates
       ├─ Viewer: score (pixel distance, kind priority, depth)
       └─ apply hold-override filter if any
  → PickResult + update marker + cursor tip
  → Tool move: preview from PickResult.point
  → Tool press: resolve again (same function) then commit
```

### Integration points

- **Single entry:** `AccuSnap::resolve(...)` called by tools (and only there for modeling picks).
- Do **not** fork snap logic across `VulkanWindow` and `MainWindow` dual mouse paths; both already funnel into `CommandManager` / `ITool`.
- `CommandContext` carries `SnapSettings*` and `SnapSession*` (or equivalent owned by `MainWindow` and injected in `make_command_context()`).

---

## §2 Snap kinds, priority, and algorithm bounds

### Enabled kinds (Phase 1)

| SnapKind | Source | Algorithm bound |
|----------|--------|-----------------|
| Endpoint | `Edge.v0` / `Edge.v1` | World vertex position; dedupe by position tolerance |
| Midpoint | `Edge` | Line midpoint, or curve eval at `t = (t0+t1)/2` when curve exists |
| Center | Face / sphere feature | Planar face: outer-loop centroid or AABB center; sphere: feature / analytic center |
| Intersection | Edge–edge | **Line–line only**; treat as intersect if closest distance &lt; topology tolerance |
| Perpendicular | Edge + reference | **Line only**; foot from `SnapSession::last_point` (or active rubber origin); skip if no reference |
| Nearest | Edge | **Line only**; closest point clamped to segment; soft fallback when near an edge |
| Grid | Viewer workplane | Quantize to `grid_spacing` on default `y=0` (or current workplane UV) |
| Workplane | Viewer | Ray ∩ workplane when nothing else wins; same role as today's ground hit |

### Explicitly deferred

Tangent, parallel, extension intersections, face–face curves, curve–curve intersections, quadrant, “selected-only” filters.

### Viewer scoring (inside aperture)

1. Smaller screen-pixel distance wins (primary).
2. Kind priority (configurable; default):  
   `Endpoint > Midpoint > Intersection > Center > Perpendicular > Nearest > Grid > Workplane`
3. Tie-break: smaller ray `t` (closer in depth).
4. Hold override: keep only that kind (or force it to top weight).

### Kernel query contract

- **In:** bodies (or part + guids), optional near point / ray, enabled kinds, linear tolerance.
- **Out:** `vector<SnapCandidate>` in world space.
- Kernel does **not** apply pixel aperture, grid, or final UI priority.
- Viewer **may** coarse-filter bodies with mesh pick for performance; correctness must not depend on mesh vertices as snap truth.

### Tool semantics

- Sphere / copy / box base corners: replace ad-hoc ground/mesh hits with `PickResult.point`.
- Box height step: tool-local vertical-plane constraint may remain; AccuSnap on height is best-effort in Phase 1. Base points **must** use AccuSnap.
- `Perpendicular` uses `session.last_point`; clear on tool `on_start`; update after each successful commit pick.
- Fallback: no aperture candidate → workplane → else existing “miss” status messaging.

---

## §3 UI, settings, override keys, feedback

### Settings

| Control | Behavior |
|---------|----------|
| Master snap switch | Off → resolve skips object kinds; workplane only (grid follows master off by default) |
| Kind checkboxes | Endpoint, Midpoint, Center, Intersection, Perpendicular, Nearest, Grid |
| Aperture | Pixels; default ~12; range e.g. 4–40 |
| Grid spacing | Model units; default `1.0` when Grid on |

**Placement:** Tools / Modeling “Snap Settings…” dialog + toolbar master toggle.  
**Persistence (Phase 1):** `QSettings` (session/user), not XL.

### Hold-to-override keys (while held)

| Key | Override |
|-----|----------|
| `E` | Endpoint only |
| `M` | Midpoint only |
| `C` | Center only |
| `I` | Intersection only |
| `P` | Perpendicular only |
| `G` | Grid only |
| `F3` or `` ` `` | Toggle master switch (tap, not hold) |

While a modeling tool owns the viewport, snap overrides take priority over conflicting camera shortcuts.

### Feedback

1. **Snap marker** in the viewport at the winning point (kind-specific glyph: endpoint square, midpoint triangle, center circle, intersection ×, perpendicular ⊥, nearest dash, grid +).
   - Prefer a dedicated snap-overlay channel so tool previews are not clobbered.
   - Acceptable Phase 1 shortcut: AccuSnap appends marker segments into an overlay mesh the window draws after tool preview.
2. **Cursor tip:** keep tool `prompt()`; append / second line with localized snap name when snapped.
3. **Status bar (optional):** `Snap: Midpoint`; misses keep existing `report_status` patterns.

Tool prompts stay step-focused; snap detail lives in tip/marker, not prompt spam.

### AccuDraw UI

Do not ship dynamic-input UI. Settings may show a disabled “Dynamic input (coming soon)” row. `SnapSession` keeps reserved `origin` / `axes` / `dynamic_input_mode` fields.

### i18n

All settings labels, tip snap names, and user-visible strings use `tr()` + `xcad_zh_CN.ts`.

---

## §4 Tool integration, testing, and rollout

### Tool migration

1. Add `AccuSnap::resolve` + settings/session on `CommandContext`.
2. Refactor in order:
   - `CreateSphereTool::pick_point`
   - `CreateBoxTool::pick_ground` (base steps)
   - `CopyTool::pick_ground`
   - Box height: optional follow-up; document if still plane-only.
3. Remove duplicated mesh/ground pick once callers are migrated (keep low-level `picking.hpp` ray/plane helpers).

### Suggested module layout

```text
kernel/include/api/snap.hpp
kernel/include/brep/snap/...   # or src-only internals
kernel/src/snap/...

apps/viewer/commands/snap/accusnap.hpp|.cpp
apps/viewer/commands/snap/snap_settings.hpp|.cpp
apps/viewer/commands/snap/snap_overlay.hpp|.cpp   # markers
apps/viewer/ui/...             # settings dialog + toolbar toggle
```

### Testing

| Level | Coverage |
|-------|----------|
| Kernel unit / smoke | Endpoint/midpoint on box; center on sphere; line–line intersection; perpendicular foot; tolerance dedupe |
| Viewer logic (headless where possible) | Aperture filter + priority ranking with synthetic candidates |
| Manual smoke | Create box on endpoint/midpoint; sphere center on box corner; grid spacing; hold `E`/`M`; master off; language switch for tip strings |

### Phase rollout

| Phase | Deliverable |
|-------|-------------|
| **1a** | Kernel candidates: Endpoint, Midpoint, Center; Viewer resolve + workplane; migrate sphere/box base/copy; marker + tip |
| **1b** | Intersection (line–line), Perpendicular, Nearest; settings dialog + kind toggles + override keys |
| **1c** | Grid snap + `QSettings` persistence + polish overlay channel |
| **2** | AccuDraw: origin/axes from last point / view, ortho lock, coordinate key-in, integrate with resolve |

### AccuDraw reservation (Phase 2 contract)

- `SnapSession` already holds `origin` / `axes`.
- Future: after object snap wins, AccuDraw may still constrain the point onto axes / polar distance without changing Kernel SnapQuery.
- Tools continue to see a single `PickResult`; AccuDraw is a filter/stage inside Viewer `resolve`, not a second pick API.

### Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Sphere/box tessellation ≠ B-Rep centers | Prefer feature/analytic center for spheres; face centroid from topology loops |
| Dual mouse paths diverge | Only tools call `resolve` |
| Preview fights snap marker | Dedicated overlay or strict draw order |
| Key conflicts | Tool-active override policy documented |
| Perf on many edges | Mesh coarse body filter; defer spatial index to later |

---

## Success criteria (Phase 1 done)

- Creating box/sphere/copy can snap to B-Rep endpoints/midpoints/centers with visible marker + tip.
- Intersection / perpendicular / nearest / grid work per §2 bounds.
- Master switch, kind toggles, and hold overrides behave as §3.
- No AccuDraw UI shipped; reserved fields/hooks exist and are documented.
- Kernel snap geometry covered by automated tests; viewer manual smoke checklist passes.

## Open items (acceptable defaults)

- Exact toolbar iconography for master toggle.
- Whether box height step gets full AccuSnap in 1a or later (default: later).
- Final aperture default (12px) may be tuned after dogfooding.
