# Trimmed-Face CDT Tessellation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace plane ear-clip / full-sphere UV grids with a self-hosted parametric-domain CDT so multi-outer, multi-inner, edge-touching holes, and trimmed sphere faces tessellate correctly — fixing `untitled.xl` Sphere∪Box display.

**Architecture:** Sample face loops into UV, group each Outer with its Inners, run incremental Bowyer–Watson Delaunay then recover constraint edges, delete exterior/hole triangles, map remaining UV triangles back through `Surface::eval`. Topology allows ≥1 Outer; `Face::outer_loops()` exposes them.

**Tech Stack:** C++20, CMake/Ninja (MinGW), GoogleTest, Eigen via existing `brep` math types; no third-party triangulators (ADR 0005).

## Global Constraints

- Spec authority: `docs/superpowers/specs/2026-08-11-trimmed-face-cdt-tessellation-design.md`.
- No third-party mesh libraries (earcut / poly2tri / Triangle / OCCT).
- Public APIs `tessellate_face` / `tessellate_body` / `TessellationOptions` keep existing signatures.
- Do not change boolean builders except if a test needs them; display path only.
- Cylinder trimmed tessellation is **out of this plan** (interface may accept `Surface*` but only Plane + Sphere wired).
- Build: `cmake-build-mingw-debug`; put `C:\Qt6\Tools\mingw1310_64\bin` on `PATH` when linking Qt-adjacent targets.
- Do not commit `gpp_err*.txt`, `tmp_empty.cpp`, or build-tree junk.
- TDD: failing test → implement → green → commit per task.

## Algorithm lock (this plan)

**Incremental Bowyer–Watson** for point insertion into a Delaunay triangulation of the UV plane, then **constraint recovery**: for each required segment, walk intersecting triangle edges and either Lawson-flip or insert a Steiner midpoint on the constraint until the segment is a union of mesh edges. Finally delete triangles whose centroid is outside the Outer or inside any Inner (even-odd / winding via `point_in_polygon`).

## File map

| File | Responsibility |
|------|----------------|
| `kernel/include/brep/topology.hpp` | Declare `Face::outer_loops()` |
| `kernel/src/topology.cpp` | Implement `outer_loops()` |
| `kernel/src/validate.cpp` | Outer count `>= 1` |
| `kernel/include/brep/mesh/cdt.hpp` | CDT types + `triangulate_constrained` |
| `kernel/src/mesh/cdt.cpp` | Bowyer–Watson + constraint recovery |
| `kernel/include/brep/mesh/loop_sample.hpp` | Sampled ring / region types + APIs |
| `kernel/src/mesh/loop_sample.cpp` | Edge sampling, seam unwrap, region grouping |
| `kernel/src/mesh.cpp` | Orchestrate CDT for Plane + Sphere; remove ear-clip path when Task 10 lands |
| `cmake/BrepCore.cmake` | Compile new `.cpp` into `brep_core` |
| `tests/CMakeLists.txt` | Register new test binaries |
| `tests/kernel/test_inner_loop.cpp` | Multi-outer validate |
| `tests/kernel/test_cdt.cpp` | CDT unit tests |
| `tests/kernel/test_loop_sample.cpp` | Sampling + seam |
| `tests/kernel/test_tessellate_inner.cpp` | Corner-touching hole |
| `tests/kernel/test_tessellate_trimmed_sphere.cpp` | Trimmed sphere |
| `tests/kernel/test_tessellate_untitled_union.cpp` | `untitled.xl` pose regression |

---

### Task 1: Multi-Outer topology + validate

**Files:**
- Modify: `kernel/include/brep/topology.hpp`
- Modify: `kernel/src/topology.cpp`
- Modify: `kernel/src/validate.cpp` (Outer count check ~lines 44–57)
- Modify: `tests/kernel/test_inner_loop.cpp` (`ExactlyOneOuterRequired` → allow multiple)
- Test: `tests/kernel/test_inner_loop.cpp`

**Interfaces:**
- Consumes: existing `Face::loops`, `LoopType`
- Produces: `std::vector<Loop*> Face::outer_loops() const;` — all loops with `type == Outer`, stable order = `loops` order. `outer_loop()` remains first Outer or `nullptr`.

- [ ] **Step 1: Rewrite the failing validate expectation**

Replace `TEST(InnerLoop, ExactlyOneOuterRequired)` with:

```cpp
TEST(InnerLoop, MultipleOutersAllowed) {
  Model model;
  Body* body = make_planar_face_with_hole(model);
  Face* face = body->shells[0]->faces[0];
  face->loops[1]->type = LoopType::Outer;  // two Outers, zero Inner

  const auto report = validate_body(*body);
  EXPECT_TRUE(report.ok()) << "multi-outer must validate";
  ASSERT_EQ(face->outer_loops().size(), 2u);
}
```

Add declaration usage — test will fail to compile until `outer_loops` exists; that is OK — add a compile-only stub next if needed. Prefer: first change validate only and keep test asserting `ok()`, then add `outer_loops` in same task.

Also keep:

```cpp
TEST(InnerLoop, MissingOuterStillErrors) {
  Model model;
  Body* body = make_planar_face_with_hole(model);
  Face* face = body->shells[0]->faces[0];
  for (Loop* l : face->loops) l->type = LoopType::Inner;
  EXPECT_FALSE(validate_body(*body).ok());
}
```

- [ ] **Step 2: Run test to verify current behavior fails new expectation**

```powershell
$env:PATH = "C:\Qt6\Tools\mingw1310_64\bin;" + $env:PATH
Set-Location E:\brepkernelstudy
cmake --build cmake-build-mingw-debug --target brep_test_inner_loop -j 8
.\cmake-build-mingw-debug\bin\brep_test_inner_loop.exe --gtest_filter=InnerLoop.MultipleOutersAllowed
```

Expected: FAIL (validate still requires exactly one outer) and/or compile error if `outer_loops` missing.

- [ ] **Step 3: Implement `outer_loops` + validate change**

In `topology.hpp` inside `Face`:

```cpp
[[nodiscard]] std::vector<Loop*> outer_loops() const;
```

In `topology.cpp`:

```cpp
std::vector<Loop*> Face::outer_loops() const {
  std::vector<Loop*> outers;
  for (Loop* l : loops) {
    if (l && l->type == LoopType::Outer) outers.push_back(l);
  }
  return outers;
}
```

In `validate.cpp` replace the `outer_count != 1` error block with:

```cpp
if (outer_count == 0) {
  report.error(fn, "missing outer loop");
  continue;
}
// outer_count >= 1 is OK (multi-outer allowed)
```

Delete the old `"exactly one outer loop required"` branch entirely.

- [ ] **Step 4: Run tests**

```powershell
.\cmake-build-mingw-debug\bin\brep_test_inner_loop.exe
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```powershell
git add kernel/include/brep/topology.hpp kernel/src/topology.cpp kernel/src/validate.cpp tests/kernel/test_inner_loop.cpp
git commit -m "Allow multiple outer loops on a face."
```

---

### Task 2: CDT mesh + unconstrained Delaunay

**Files:**
- Create: `kernel/include/brep/mesh/cdt.hpp`
- Create: `kernel/src/mesh/cdt.cpp`
- Modify: `cmake/BrepCore.cmake` (add `mesh/cdt.cpp`)
- Create: `tests/kernel/test_cdt.cpp`
- Modify: `tests/CMakeLists.txt` (register `brep_test_cdt`)

**Interfaces:**
- Consumes: `brep::Point2d` from `brep/math.hpp`
- Produces:

```cpp
namespace brep::mesh {
struct CdtVertex { Point2d uv; };
struct CdtTriangle { int v[3]; };  // CCW in UV
struct CdtResult {
  std::vector<CdtVertex> vertices;
  std::vector<CdtTriangle> triangles;
  bool ok{false};
  std::string diagnostics;
};

/// Triangulate points + optional constraint segments (vertex index pairs).
/// Empty constraints ⇒ unconstrained Delaunay of the point set (plus super-tri cull).
[[nodiscard]] CdtResult triangulate_constrained(
    const std::vector<Point2d>& points,
    const std::vector<std::pair<int, int>>& constraints,
    double eps = 1e-12);
}
```

- [ ] **Step 1: Write failing unit test (square → 2 triangles)**

`tests/kernel/test_cdt.cpp`:

```cpp
#include "brep/mesh/cdt.hpp"
#include <gtest/gtest.h>

using brep::Point2d;
using brep::mesh::triangulate_constrained;

TEST(Cdt, UnconstrainedSquareTwoTriangles) {
  std::vector<Point2d> pts = {
      {0, 0}, {1, 0}, {1, 1}, {0, 1},
  };
  const auto r = triangulate_constrained(pts, {});
  ASSERT_TRUE(r.ok) << r.diagnostics;
  EXPECT_EQ(r.triangles.size(), 2u);
  // All vertices referenced in range
  for (const auto& t : r.triangles) {
    for (int k = 0; k < 3; ++k) {
      EXPECT_GE(t.v[k], 0);
      EXPECT_LT(t.v[k], static_cast<int>(r.vertices.size()));
    }
  }
}
```

Register in `tests/CMakeLists.txt` (mirror other tests):

```cmake
add_executable(brep_test_cdt kernel/test_cdt.cpp)
target_link_libraries(brep_test_cdt PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_cdt
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_cdt>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_cdt."
)
```

- [ ] **Step 2: Run test — expect link/compile fail**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt -j 8
```

Expected: FAIL (missing symbols / file).

- [ ] **Step 3: Minimal Bowyer–Watson implementation**

Create `cdt.hpp` with the API above.

In `cdt.cpp` implement at least:
1. Bounding super-triangle covering all points (+ margin).
2. Insert each input point; locate triangle containing point (walk or linear scan); Bowyer–Watson cavity; retriangulate to point.
3. Remove any triangle that still touches a super-vertex.
4. Ignore `constraints` for this task (accept empty only; if non-empty, set `ok=false` with message — next task fills in).

Add to `cmake/BrepCore.cmake`:

```cmake
${BREP_KERNEL_DIR}/src/mesh/cdt.cpp
```

- [ ] **Step 4: Run test — expect PASS**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt -j 8
.\cmake-build-mingw-debug\bin\brep_test_cdt.exe --gtest_filter=Cdt.UnconstrainedSquareTwoTriangles
```

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add kernel/include/brep/mesh/cdt.hpp kernel/src/mesh/cdt.cpp cmake/BrepCore.cmake tests/kernel/test_cdt.cpp tests/CMakeLists.txt
git commit -m "Add parametric CDT scaffold with unconstrained Delaunay."
```

---

### Task 3: Constraint edges + polygonal hole

**Files:**
- Modify: `kernel/src/mesh/cdt.cpp`
- Modify: `tests/kernel/test_cdt.cpp`

**Interfaces:**
- Consumes: `triangulate_constrained` from Task 2
- Produces: same API; constraints honored; triangles only inside outer when caller passes outer+hole edges and then filters — **for this task**, CDT returns full Delaunay of points with constraints present as edges; add helper:

```cpp
[[nodiscard]] CdtResult triangulate_polygon_with_holes(
    const std::vector<Point2d>& outer_ccw,
    const std::vector<std::vector<Point2d>>& holes_cw,
    double eps = 1e-12);
```

Implementation: concatenate vertices, build consecutive constraint edges for each ring (close last→first), call `triangulate_constrained`, then drop triangles whose centroid fails `in_outer && !in_any_hole`.

- [ ] **Step 1: Failing test — square with square hole**

```cpp
TEST(Cdt, SquareWithHoleKeepsBoundaryAndDropsInterior) {
  std::vector<Point2d> outer = {{0,0},{4,0},{4,4},{0,4}};
  std::vector<Point2d> hole = {{1,1},{1,3},{3,3},{3,1}};  // CW
  const auto r = brep::mesh::triangulate_polygon_with_holes(outer, {hole});
  ASSERT_TRUE(r.ok) << r.diagnostics;
  ASSERT_FALSE(r.triangles.empty());
  const Point2d hole_c{2, 2};
  const Point2d solid_c{0.5, 0.5};
  auto tri_contains = [&](Point2d p) {
    for (const auto& t : r.triangles) {
      const auto& a = r.vertices[t.v[0]].uv;
      const auto& b = r.vertices[t.v[1]].uv;
      const auto& c = r.vertices[t.v[2]].uv;
      // barycentric or same side test
      // ...
    }
    return false;
  };
  EXPECT_FALSE(tri_contains(hole_c));
  EXPECT_TRUE(tri_contains(solid_c));
}
```

Implement the barycentric helper fully in the test file (copy pattern from `test_tessellate_inner.cpp`).

- [ ] **Step 2: Run — expect FAIL**

```powershell
.\cmake-build-mingw-debug\bin\brep_test_cdt.exe --gtest_filter=Cdt.SquareWithHoleKeepsBoundaryAndDropsInterior
```

Expected: FAIL (`ok=false` or hole covered).

- [ ] **Step 3: Implement constraint recovery + polygon helper**

In `cdt.cpp`:
- For each constraint `(i,j)`: while segment not in mesh, find an intersecting edge; if flippable and flip brings endpoints closer to being connected, flip; else insert midpoint Steiner on the constraint, split, continue.
- Mark constrained edges so flips never destroy them once recovered.
- Implement `point_in_polygon` (ray cast) for filtering.
- Expose `triangulate_polygon_with_holes` in `cdt.hpp`.

- [ ] **Step 4: Run — expect PASS**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt -j 8
.\cmake-build-mingw-debug\bin\brep_test_cdt.exe
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```powershell
git add kernel/include/brep/mesh/cdt.hpp kernel/src/mesh/cdt.cpp tests/kernel/test_cdt.cpp
git commit -m "Recover CDT constraints and triangulate polygons with holes."
```

---

### Task 4: Loop edge sampling (line + circle)

**Files:**
- Create: `kernel/include/brep/mesh/loop_sample.hpp`
- Create: `kernel/src/mesh/loop_sample.cpp`
- Modify: `cmake/BrepCore.cmake`
- Create: `tests/kernel/test_loop_sample.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Loop`, `Edge`, `Curve`, `TessellationOptions`, `Surface` (`PlaneSurface` / `SphereSurface` `param_of`)
- Produces:

```cpp
namespace brep::mesh {
struct SampledPoint {
  Point3d xyz;
  Point2d uv;
};
struct SampledRing {
  LoopType type{LoopType::Outer};
  std::vector<SampledPoint> points;  // closed: last may equal first or not; do NOT duplicate first at end
};

/// Sample one loop into UV of `surface` (must be Plane or Sphere for now).
[[nodiscard]] SampledRing sample_loop(const Loop& loop, const Surface& surface,
                                      const TessellationOptions& opts);

/// Chord-height samples along one edge in the coedge sense.
[[nodiscard]] std::vector<Point3d> sample_edge_xyz(const CoEdge& ce,
                                                   const TessellationOptions& opts);
}
```

Sampling rules:
- `LineCurve`: at least endpoints; if length large vs `linear_deflection`, subdivide uniformly so chord error for a straight line is 0 (still OK to just use endpoints).
- `CircleCurve`: choose `n = max(2, ceil(Δθ / α))` where `α` from angular deflection and from `2*acos(1 - h/R)` with `h = linear_deflection` (fallback `0.02*R`).
- Project each XYZ with `PlaneSurface::param_of` or `SphereSurface::param_of`.

- [ ] **Step 1: Failing test — quarter circle gets >2 samples**

```cpp
TEST(LoopSample, QuarterCircleHasInteriorSamples) {
  Model model;
  // Build a single open-ish loop: center C, arc from +X to +Y on unit circle in XY
  // (minimal Face+Loop with one CircleCurve edge + two radii optional).
  // Assert sample_edge_xyz on the arc coedge has size() >= 4 for default opts.
}
```

Construct using `model.make_circle`, `make_edge` with `t0=0`, `t1=pi/2`, vertices at `(1,0,0)` and `(0,1,0)`.

- [ ] **Step 2: Run — expect FAIL**

- [ ] **Step 3: Implement sampling**

Wire `sample_edge_xyz` / `sample_loop`. For sphere `param_of`, normalize `u` into `[0, 2π)`.

- [ ] **Step 4: PASS + register `brep_test_loop_sample`**

- [ ] **Step 5: Commit**

```powershell
git commit -m "Add deflection-based loop edge sampling for tessellation."
```

---

### Task 5: Region grouping + plane faces via CDT

**Files:**
- Modify: `kernel/src/mesh/loop_sample.cpp` / `.hpp` (add `group_face_regions`)
- Modify: `kernel/src/mesh.cpp` (`tessellate_plane_face` → CDT path)
- Test: `tests/kernel/test_tessellate_inner.cpp` (existing must stay green)

**Interfaces:**
- Consumes: `Face::outer_loops()`, `inner_loops()`, `sample_loop`, `triangulate_polygon_with_holes`
- Produces:

```cpp
struct FaceRegion {
  SampledRing outer;
  std::vector<SampledRing> holes;
};
[[nodiscard]] std::vector<FaceRegion> group_face_regions(
    const Face& face, const Surface& surface, const TessellationOptions& opts);
```

Assignment: hole centroid UV → `point_in_polygon(outer)`; if none match, `BREP_WARN` and skip hole.

`tessellate_plane_face`:
1. `group_face_regions`
2. For each region, `triangulate_polygon_with_holes`
3. Append 3D verts with plane normal + normalized UV
4. Respect face sense (existing flip logic)

- [ ] **Step 1: Ensure existing hole test still expresses intent**

No change required if already present; run:

```powershell
.\cmake-build-mingw-debug\bin\brep_test_tessellate_inner.exe
```

After switching implementation, it must still PASS. If you switch before it passes, fix CDT filtering.

- [ ] **Step 2: Temporarily break by calling empty CDT** — optional; prefer direct replace and watch `HoleCenterNotCovered`.

- [ ] **Step 3: Replace `tessellate_plane_face` body** to use CDT. Keep old `bridge_hole` / `ear_clip` functions in file for now (deleted in Task 10).

- [ ] **Step 4: Run**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_tessellate_inner -j 8
.\cmake-build-mingw-debug\bin\brep_test_tessellate_inner.exe
```

Expected: PASS (`HoleCenterNotCovered`).

- [ ] **Step 5: Commit**

```powershell
git commit -m "Tessellate planar faces with parametric CDT."
```

---

### Task 6: Corner-touching hole (Sphere∪Box plane case)

**Files:**
- Modify: `tests/kernel/test_tessellate_inner.cpp`
- Modify: `kernel/src/mesh/cdt.cpp` / `loop_sample.cpp` if shared-vertex merge needed

**Interfaces:**
- Consumes: plane tessellation from Task 5
- Produces: correct mesh when Inner shares the Outer corner vertex

- [ ] **Step 1: Failing test**

Build a sheet face: Outer unit square `[0,1]²`; Inner triangle `(0,0) → (0.5,0) → (0,0.5)` (CW) sharing corner `(0,0)` — same `Vertex*` for outer and inner at corner.

```cpp
TEST(TessellateInner, CornerTouchingHoleNotCovered) {
  // ... build topology ...
  const TriangleMesh mesh = tessellate_body(*body);
  const Point3d inside_hole{0.15, 0.15, 0};
  const Point3d outside{0.8, 0.8, 0};
  // assert hole point not in any triangle; outside is
}
```

- [ ] **Step 2: Run — expect FAIL** if CDT duplicates corner UV without merging or filters wrong.

- [ ] **Step 3: Fix** — when building CDT input, merge UV points within `eps` (e.g. `1e-9` relative to bbox); do not insert bridge edges.

- [ ] **Step 4: PASS**

- [ ] **Step 5: Commit**

```powershell
git commit -m "Support corner-touching inner loops in planar CDT tessellation."
```

---

### Task 7: Sphere UV seam unwrap

**Files:**
- Modify: `kernel/include/brep/mesh/loop_sample.hpp`
- Modify: `kernel/src/mesh/loop_sample.cpp`
- Modify: `tests/kernel/test_loop_sample.cpp`

**Interfaces:**
- Produces:

```cpp
/// Make ring UV contiguous: if |Δu|>π between adjacent samples, shift by ±2π
/// so the polyline does not jump the seam. May expand u outside [0,2π).
[[nodiscard]] SampledRing unwrap_sphere_ring(SampledRing ring);
```

- [ ] **Step 1: Failing test**

```cpp
TEST(LoopSample, SphereRingAcrossSeamIsContiguous) {
  SampledRing r;
  r.points = {
    {{}, Point2d{0.1, 0.0}},
    {{}, Point2d{6.2, 0.0}},  // near 2π
  };
  // After treating as adjacent on a short arc across seam, unwrap should
  // yield |u1-u0| < π (e.g. second becomes -0.083 or first += 2π).
  auto u = unwrap_sphere_ring(r);
  EXPECT_LT(std::abs(u.points[1].uv.u() - u.points[0].uv.u()),
            std::numbers::pi);
}
```

Adjust fixture to three points clearly crossing the seam.

- [ ] **Step 2: FAIL**

- [ ] **Step 3: Implement sequential unwrap** — for `i=1..n-1`, while `u[i]-u[i-1] > π` subtract `2π`; while `< -π` add `2π`. Close ring carefully (compare last to first).

- [ ] **Step 4: PASS**

- [ ] **Step 5: Commit**

```powershell
git commit -m "Unwrap sphere loop UV across the periodic seam."
```

---

### Task 8: Trimmed sphere face tessellation

**Files:**
- Modify: `kernel/src/mesh.cpp` — replace `tessellate_sphere_face` full grid with CDT path when `outer_loop()` exists
- Create: `tests/kernel/test_tessellate_trimmed_sphere.cpp`
- Modify: `tests/CMakeLists.txt`
- Keep full closed sphere (`make_sphere` single face with seam) working: if the only outer samples the full domain, CDT still fills the sphere; alternatively detect closed analytic sphere (existing path) when loops are the standard seam+poles. **Rule for this task:** always use CDT from sampled loops; update `brep_test_tessellate_sphere` if counts change but normals/coverage must remain valid.

**Interfaces:**
- Consumes: `group_face_regions` + `unwrap_sphere_ring` + `triangulate_polygon_with_holes`
- For each UV vertex: `xyz = sphere.eval(u_mod, v)`, `u_mod = fmod(u, 2π)` normalized to `[0,2π)`; normal via `face.normal_at`

- [ ] **Step 1: Failing test — ⅞ ball outer has no points in deleted octant**

Reuse octant topology from `build_axis_octant_ball` path: call boolean Subtract Sphere−Box at origin, or build the same 4-face body; tessellate; pick a point deep in the removed +++ octant on the sphere surface; assert not covered. Also:

```cpp
EXPECT_LT(mesh.vertices.size(), 300u);  // full default sphere is ~325
```

Tune threshold after measuring.

- [ ] **Step 2: FAIL** (current code draws full sphere ⇒ point covered / vert count high)

- [ ] **Step 3: Implement trimmed sphere tessellation**

Delete/stop calling the nu×nv grid for faces that have loops. Closed `make_sphere` still has one outer loop around the seam — sampling must cover the full sphere domain so CDT fills it (may need both poles + dense equator). If closed sphere quality regresses, add Steiner grid points **inside** the UV domain that pass the inside-outer test (optional densify using `TessellationOptions` segment counts as a UV lattice clipped by the outer).

- [ ] **Step 4: Run**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_tessellate_trimmed_sphere brep_test_tessellate_sphere -j 8
.\cmake-build-mingw-debug\bin\brep_test_tessellate_trimmed_sphere.exe
.\cmake-build-mingw-debug\bin\brep_test_tessellate_sphere.exe
```

Expected: both PASS.

- [ ] **Step 5: Commit**

```powershell
git commit -m "Tessellate trimmed sphere faces with seam-aware CDT."
```

---

### Task 9: `untitled.xl` Sphere∪Box tessellation regression

**Files:**
- Create: `tests/kernel/test_tessellate_untitled_union.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `evaluate` boolean Union + `tessellate_body`

- [ ] **Step 1: Failing/regression test with exact pose**

```cpp
TEST(TessellateUntitledUnion, CornerSphereBoxLooksTrimmed) {
  Model model;
  Body* box = make_box(model, BoxSpec{
      .min = {-2.38421, 0, 4.59474},
      .max = {-1.57147, 0.826297, 5.26894},
      .name = "box_copy_copy"});
  Body* sphere = make_sphere(model, SphereSpec{
      .center = {-1.57147, 0.826297, 4.59474},
      .radius = 0.413149,
      .name = "sphere"});
  auto eval = boolean::make_default_boolean_evaluator();
  const auto result =
      eval->evaluate(boolean::BooleanOp::Union, model, *box, *sphere, {});
  ASSERT_TRUE(result.ok()) << result.diagnostics;
  ASSERT_NE(result.body, nullptr);
  ASSERT_TRUE(validate_body(*result.body).ok());

  const TriangleMesh mesh = tessellate_body(*result.body);
  EXPECT_FALSE(mesh.indices.empty());
  // Must not be ~full sphere + broken planes: vertex count well below
  // prior broken ~364 if planes failed, but sphere patch + box should be
  // finite. Stronger checks:
  // 1) A point on the box top face away from the corner hole is covered.
  // 2) Sphere center itself is not a mesh vertex cluster implying full ball —
  //    pick a point on the sphere in the inward octant (inside the box) and
  //    assert it is NOT on the outer mesh.
  const Point3d inward_on_sphere =
      Point3d{-1.57147, 0.826297, 4.59474} +
      0.413149 * Vector3d{-1, -1, 1}.normalized();  // inward octant direction
  // point_in_any_triangle(inward_on_sphere) == false
}
```

Refine the inward direction to match `detect_corner_octant` signs for this corner (`sx=-1,sy=-1,sz=+1`).

- [ ] **Step 2: Run against current main** — if Tasks 5–8 done, may already PASS; if not, FAIL documents the bug.

- [ ] **Step 3: Fix any remaining filtering/sampling gaps until PASS**

- [ ] **Step 4: Also run curved boolean + tessellate suite**

```powershell
.\cmake-build-mingw-debug\bin\brep_test_sphere_curved_boolean.exe --gtest_filter=SphereBoxBoolean.*
.\cmake-build-mingw-debug\bin\brep_test_tessellate_untitled_union.exe
```

- [ ] **Step 5: Commit**

```powershell
git commit -m "Add untitled.xl sphere-box union tessellation regression."
```

---

### Task 10: Remove ear-clip path + doc cross-links

**Files:**
- Modify: `kernel/src/mesh.cpp` — delete unused `bridge_hole`, `ear_clip_triangulate`, `ensure_ccw/cw` if only used by old path (keep helpers still needed)
- Modify: `docs/superpowers/specs/2026-08-10-analytic-sphere-brep-boolean-design.md` — short note under Phase 1 / tessellation pointing to CDT spec
- Modify: `docs/superpowers/specs/2026-08-11-trimmed-face-cdt-tessellation-design.md` — status → Implemented (date)

- [ ] **Step 1: Confirm no tests call old symbols** (grep `bridge_hole`)

- [ ] **Step 2: Delete dead code; build all mesh-related tests**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt brep_test_loop_sample brep_test_tessellate_inner brep_test_tessellate_sphere brep_test_tessellate_trimmed_sphere brep_test_tessellate_untitled_union brep_test_inner_loop -j 8
```

- [ ] **Step 3: Run all of the above exes — all PASS**

- [ ] **Step 4: Manual viewer check (human):** open `C:/Users/xingbl/Desktop/untitled.xl`, Fuse the two GUIDs; expect solid box + spherical cap.

- [ ] **Step 5: Commit**

```powershell
git commit -m "Remove ear-clip tessellation path; link CDT design as implemented."
```

---

## Spec coverage checklist

| Spec requirement | Task |
|------------------|------|
| Multi Outer validate + `outer_loops()` | 1 |
| CDT data structures + triangulate | 2–3 |
| Deflection loop sampling | 4 |
| Region grouping / multi hole plane | 5 |
| Corner-touching hole | 6 |
| Sphere seam unwrap | 7 |
| Trimmed sphere (no full ball) | 8 |
| `untitled.xl` regression | 9 |
| No third-party lib / remove ear-clip | 10 + Global Constraints |
| Cylinder | Explicitly deferred (non-goal) |

## Placeholder / consistency self-review

- Algorithm locked to Bowyer–Watson + constraint recovery (no TBD).
- API names consistent: `triangulate_constrained`, `triangulate_polygon_with_holes`, `sample_loop`, `group_face_regions`, `unwrap_sphere_ring`.
- Test binary names match `tests/CMakeLists.txt` patterns.
- Build directory and PATH match repo practice.
