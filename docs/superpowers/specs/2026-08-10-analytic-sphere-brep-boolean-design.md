# Analytic Sphere + General B-Rep Boolean — Technical Implementation Plan

Date: 2026-08-10  
Status: Draft for review  
Branch: `cursor/modern-cpp-brep-kernel`

## Goals

1. **Smooth spheres** like typical CAD/MicroStation display: geometry is an **analytic sphere**, display uses **adaptive / tolerance-driven tessellation** (smooth shading normals).
2. **General curved-surface B-Rep boolean** (union / subtract / intersect) with an in-house evaluator whose **pipeline is inspired by OpenCASCADE concepts** (no OCCT link/dependency in Phase 1–N unless later decided).

## Non-goals (near term)

- Linking or vendoring OpenCASCADE binaries
- NURBS general boolean (deferred until plane/sphere/cylinder path is solid)
- Assembly-instance boolean
- Full non-manifold healing suite
- Mesh-only boolean as the primary result (mesh may remain a debug/fallback visualization only)

## Decisions (locked from product discussion)

| Topic | Choice |
|-------|--------|
| Sphere look | Analytic `SphereSurface` + better display tessellation |
| Boolean target | General curved B-Rep boolean (in-house) |
| Mesh boolean | Not primary; optional debug only |
| External kernel | Ideas from OCCT only; self-implemented |
| UI (boolean) | Select two objects → menu Union / Subtract / Intersect |
| Operands after boolean | Suppress operand features; keep result |

---

## Current baseline (gaps)

| Area | Today |
|------|--------|
| Sphere body | `make_sphere` builds a **UV faceted planar-triangle shell** |
| Surfaces | `PlaneSurface` only in solids; `SurfaceKind` has Cylinder/Nurbs enums unused |
| Curves | `LineCurve`, `CircleCurve` (eval); solids use lines almost exclusively |
| Tessellation | `tessellate_body` fans **planar** outer loops only |
| Boolean | None (no intersect / split / classify / sew) |
| Features | Box, Sphere, Sketch, Extrude + history/XL |

---

## Architecture overview

```text
┌─────────────────────────────────────────────────────────────┐
│ Viewer: select 2 → boolean.union|subtract|intersect         │
│ BooleanFeature + suppress operands + regen + undo           │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│ Part / Regenerator                                           │
│  BooleanFeature::rebuild → IBooleanEvaluator                 │
└───────────────────────────┬─────────────────────────────────┘
                            │
        ┌───────────────────┴───────────────────┐
        ▼                                       ▼
┌───────────────────┐                 ┌─────────────────────┐
│ Geometry kernel   │                 │ Boolean pipeline      │
│ SphereSurface     │                 │ (OCCT-inspired stages)│
│ CircleCurve       │                 │ Intersect→Split→      │
│ Tessellator       │                 │ Classify→Build        │
└───────────────────┘                 └─────────────────────┘
```

---

## Part A — Analytic sphere + display tessellation

### A1. Geometry types

Extend `SurfaceKind` / implement:

```text
SurfaceKind += Sphere   // or reuse a dedicated kind; avoid overloading Nurbs

class SphereSurface : public Surface {
  Point3d center;
  double radius;
  // Local frame for UV: u = longitude [0,2π), v = latitude [-π/2, π/2]
  eval(u,v), normal(u,v), param_of(point)  // for projection / classification
};
```

Ensure `CircleCurve` is first-class for sphere seams / parallels when needed.

### A2. Topology for analytic sphere (B-Rep, not triangle shell)

Recommended canonical solid sphere (OCCT-like practical topology):

- **1 Face** on `SphereSurface` (or 2 hemispheres if seam handling prefers)
- **Seam edge(s)** + poles as vertices (degenerate edges at poles are a known hard case)

**Pragmatic Phase A topology** (recommended for this codebase first):

- South / north pole vertices
- One meridional **seam** edge (circle meridian or line in param space)
- One spherical face with outer loop covering the sphere (periodic seam)

Exact pole/seam scheme to be fixed in a short ADR note during implementation; success criterion is: **Body validates as closed solid**, tessellator produces smooth mesh, snap Center uses analytic center.

**Migrate** `make_sphere` / `SphereFeature::rebuild` off faceted planar triangles onto this analytic body.

### A3. Tessellation

Replace planar-only assumption in `tessellate_body`:

```text
TessellationOptions {
  linear_deflection;   // max chord error
  angular_deflection;  // max normal angle
  min_segments_u/v;
}

tessellate_face(Face) →
  if Plane → existing fan
  if Sphere → UV grid / recursive subdivision to meet deflection
             normals from SphereSurface::normal (smooth shading)
```

Viewer: keep uploading `TriangleMesh` with **true surface normals** (not flat face normals). Optional later: screen-size adaptive refinement.

### A4. Edge display

`extract_edges` for sphere: draw seam + optional silhouette approximation, or suppress internal seam in viewer (CAD often hides seam). Phase A: draw seam lightly or hide via render flag.

### A5. Snap / properties

- Center snap: `SphereFeature` / `SphereSurface` center (already preferred)
- Radius param unchanged
- Property panel unchanged functionally

### A6. Acceptance (Part A)

- Sphere looks smooth in ortho/perspective at default deflection
- Zoom-in still acceptable with tighter options
- XL roundtrip + parametric edit radius regenerates analytic sphere
- Manifold validate passes

---

## Part B — General B-Rep boolean (OCCT-inspired, in-house)

### B0. Conceptual mapping to OCCT

| OCCT idea | Our module |
|-----------|------------|
| `BRepAlgoAPI_Fuse/Cut/Common` | `BooleanFeature` + `BooleanOp` enum |
| Intersection | `IntTools` / `SurfaceIntersector` |
| Split / imprint | `FaceSplitter`, `EdgeSplitter` |
| Solid classifier | `SolidClassifier` (IN/OUT/ON) |
| Builder | `BooleanBuilder` sew selected faces into result Shell/Body |
| Tolerances | `BooleanContext { fuzzy, tol_3d, tol_2d }` |

**Do not copy OCCT source.** Reimplement stages behind clean interfaces; document references to public OCCT docs / textbook boolean pipelines.

### B1. Feature & UI (same delivery track as evaluators)

```text
BooleanFeature {
  op: Union | Subtract | Intersect
  target_feature_id
  tool_feature_id
}

UI: Modeling menu + toolbar — requires exactly 2 selected bodies/features
Subtract: primary selection = target, secondary = tool
On success: suppress both operands; result visible; undo restores
XL: persist BooleanFeature + suppression flags
```

### B2. Evaluator interface

```text
struct BooleanResult {
  Body* body;                 // owned by Model/Part
  BooleanEvalMode mode;     // AnalyticPair | General
  std::string diagnostics;
};

class IBooleanEvaluator {
  virtual BooleanResult evaluate(BooleanOp, const Body& a, const Body& b,
                                 const BooleanContext&) = 0;
};
```

Dispatcher may fast-path **AABB box ∪/−/∩ AABB box** as a correctness/perf special case inside the same evaluator family (still B-Rep out).

### B3. Pipeline stages (general)

```text
1. Preprocess
   - copy/transform operands into common space (identity today)
   - collect faces/edges; build bounding boxes

2. Intersection (geometry)
   - Face–Face: Plane–Plane, Plane–Sphere, Sphere–Sphere (priority order)
   - produce 3D intersection curves + pcurves on both faces
   - Edge–Face for incomplete graphs

3. Split / Imprint (topology)
   - insert vertices on edges
   - split edges/faces along intersection
   - update loops (Outer/Inner)

4. Classification
   - for each face (or face piece): IN / OUT / ON relative to other solid
   - use ray cast / winding / signed distance to analytic surfaces where possible

5. Selection by op
   - Union: keep OUT∪ON appropriately (standard CSG face keep rules)
   - Subtract (A−B): keep A faces outside B + B faces inside A (reversed)
   - Intersect: keep faces inside the other

6. Build
   - sew partners, orient shells, create Body
   - validate_body manifold checks

7. Fail soft
   - empty result, non-manifold, or unsupported surface pair → CommandResult::failed with reason
```

### B4. Geometry support matrix (phased inside Part B)

| Pair | Phase |
|------|--------|
| Plane–Plane (box–box, extrude–box) | B.1 |
| Plane–Sphere | B.2 |
| Sphere–Sphere | B.2 |
| Cylinder–* | B.3 (after `CylinderSurface`) |
| NURBS–* | Later |

**Part A (analytic sphere) is a hard prerequisite for B.2.**

### B5. Tolerances

- Global `BooleanContext::fuzzy` for nearly coincident entities
- Vertex merge distance; curve sampling for intersection approximation when analytic closed form is hard
- Prefer analytic intersection formulas for plane/sphere before numerical marching

### B6. Testing strategy

| Level | Cases |
|-------|--------|
| Unit | Plane–plane intersect line; plane–sphere circle; sphere–sphere circle/point/empty |
| Body | Box∪Box, Box−Box, Box∩Box → validate + volume/AABB sanity |
| Body | Sphere−Box (slot), Sphere∪Sphere, Box∩Sphere |
| Feature | BooleanFeature regen after editing operand radius/size; undo/redo; XL |
| Viewer | Manual: select two → ops; suppressed operands; smooth sphere display |

### B7. Acceptance (Part B general boolean)

- Fuse/Cut/Common for plane solids (boxes/extrudes) reliable
- At least one curved case (sphere vs box or sphere vs sphere) produces validated solid
- Failures are explicit (no silent corrupt bodies)
- No OCCT dependency required to build

---

## Delivery phases (execution order)

### Phase 0 — Spec/ADR freeze (this document)

- Approve topology choice for analytic sphere (seam/poles)
- Approve boolean stage interfaces

### Phase 1 — Analytic sphere + tessellation (Part A)

1. `SphereSurface` + types/factory on `Model`
2. Rewrite `make_sphere` to analytic B-Rep
3. Extend `tessellate_body` with deflection options + smooth normals
4. Migrate viewer/sphere tool (no API break for `SphereSpec`)
5. Tests: geometry eval, tessellation density, validate, XL, visual smoke

**Exit:** MicroStation-like smooth sphere in viewer.

### Phase 2 — Boolean scaffolding + plane solid boolean (Part B.1)

1. `BooleanFeature`, history, suppress operands, XL
2. Viewer commands/menus (two-object selection)
3. Plane–plane intersection + box/extrude boolean builder
4. AABB box fast path optional inside same API
5. Kernel gtests + viewer smoke

**Exit:** Real B-Rep 并/减/交 on boxes (and preferably planar extrudes).

### Phase 3 — Curved intersections + sphere boolean (Part B.2)

1. Plane–sphere / sphere–sphere intersection curves
2. Imprint on spherical faces; classifier using analytic insides
3. Sphere−Box / Sphere∪Sphere demos
4. Harden tolerances + failure diagnostics

**Exit:** Curved boolean usable for study demos; still not full CAD kernel.

### Phase 4 — Expand surfaces (optional)

- `CylinderSurface`, more intersection pairs
- Better pcurves, naming (`TopologyRef`), performance (BVH)

### Phase 5 — Decision gate

- Continue in-house toward NURBS, **or**
- Introduce OCCT as optional backend behind `IBooleanEvaluator` (same feature UI)

---

## Module / file map (planned)

```text
kernel/include/brep/geometry.hpp          # SphereSurface
kernel/include/brep/types.hpp             # SurfaceKind::Sphere
kernel/src/builder.cpp                    # analytic make_sphere
kernel/src/mesh.cpp                       # multi-surface tessellation
kernel/include/brep/bool/...              # context, op, result
kernel/src/bool/intersect_*.cpp
kernel/src/bool/split_*.cpp
kernel/src/bool/classify.cpp
kernel/src/bool/build.cpp
kernel/include/brep/feat/boolean_feature.hpp
kernel/src/feat/boolean_feature.cpp
apps/viewer/commands/...                  # boolean commands
apps/viewer/ui/main_window_menus.cpp      # menus/toolbar
docs/superpowers/specs/...                # this doc + short ADRs
```

---

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Sphere poles/seam topology fragility | Start with documented canonical layout; heavy validate tests |
| General boolean is multi-year | Strict phase exits; plane path before curved |
| Numerical instability | Analytic formulas first; fuzzy tol; refuse unsupported pairs |
| Scope creep (NURBS early) | Explicitly Phase 4+ |
| Faceted legacy spheres in old XL files | Version bump or rebuild-on-load for Sphere features |

## Effort sketch (indicative)

| Phase | Rough effort |
|-------|----------------|
| Phase 1 analytic sphere + tessellation | 1–3 weeks |
| Phase 2 boolean feature + plane boolean | 4–8 weeks |
| Phase 3 curved boolean MVP | 6–12+ weeks |
| Phase 4+ | ongoing |

(Depends on robustness bar and test depth.)

---

## Success criteria (program level)

1. Spheres display smoothly with analytic normals and deflection-based meshes.
2. User can boolean two solids (并/减/交) via selection + menu; operands suppress; undo works.
3. Plane solids boolean to validated B-Rep; at least one curved boolean case works end-to-end.
4. Pipeline stages are separable and documented with OCCT concept mapping (no OCCT link required).

## Open items (to resolve during Phase 0/1)

- Exact sphere seam/pole topology diagram
- Default deflection values for viewer quality vs perf
- Whether extrude holes / inner loops are required before Phase 2 (recommended: support Inner loops before complex Cut faces)
