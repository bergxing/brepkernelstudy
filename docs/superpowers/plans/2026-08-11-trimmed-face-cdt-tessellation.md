# 裁剪面 CDT 细分实现计划

> **面向 Agent 执行者：** 必须子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans，按任务逐步实施本计划。步骤使用复选框（`- [ ]`）语法以便跟踪。

**目标：** 用自研参数域 CDT 替换平面耳切 / 整球 UV 网格，使多 Outer、多 Inner、贴边孔及裁剪球面能正确细分——修复 `untitled.xl` 中 Sphere∪Box 的显示问题。

**架构：** 将面环采样到 UV，将每个 Outer 与其 Inner 分组，运行增量 Bowyer–Watson Delaunay 并恢复约束边，删除外部/孔内三角形，将剩余 UV 三角形通过 `Surface::eval` 映射回 3D。拓扑允许 ≥1 个 Outer；`Face::outer_loops()` 暴露它们。

**技术栈：** C++20、CMake/Ninja（MinGW）、GoogleTest、通过现有 `brep` 数学类型使用 Eigen；不引入第三方三角剖分库（ADR 0005）。

## 全局约束

- 规格权威来源：`docs/superpowers/specs/2026-08-11-trimmed-face-cdt-tessellation-design.md`。
- 不引入第三方网格库（earcut / poly2tri / Triangle / OCCT）。
- 公开 API `tessellate_face` / `tessellate_body` / `TessellationOptions` 保持现有签名。
- 除测试需要外，不修改布尔构建器；仅改显示路径。
- 圆柱裁剪细分**不在本计划内**（接口可接受 `Surface*`，但仅接入 Plane + Sphere）。
- 构建目录：`cmake-build-mingw-debug`；链接 Qt 相关目标时，将 `C:\Qt6\Tools\mingw1310_64\bin` 加入 `PATH`。
- 不要提交 `gpp_err*.txt`、`tmp_empty.cpp` 或构建树垃圾文件。
- TDD：失败测试 → 实现 → 通过 → 每任务一次提交。

## 算法锁定（本计划）

**增量 Bowyer–Watson** 将点插入 UV 平面 Delaunay 三角剖分，然后**约束恢复**：对每条必需线段，遍历与之相交的三角形边，执行 Lawson 翻转或在约束上插入 Steiner 中点，直到该线段成为网格边的并集。最后用 `point_in_polygon` 删除质心在 Outer 外或任一 Inner 内的三角形（even-odd / 绕向规则）。

## 文件映射

| 文件 | 职责 |
|------|------|
| `kernel/include/brep/topology.hpp` | 声明 `Face::outer_loops()` |
| `kernel/src/topology.cpp` | 实现 `outer_loops()` |
| `kernel/src/validate.cpp` | Outer 数量 `>= 1` |
| `kernel/include/brep/mesh/cdt.hpp` | CDT 类型 + `triangulate_constrained` |
| `kernel/src/mesh/cdt.cpp` | Bowyer–Watson + 约束恢复 |
| `kernel/include/brep/mesh/loop_sample.hpp` | 采样环 / 区域类型与 API |
| `kernel/src/mesh/loop_sample.cpp` | 边采样、缝展开、区域分组 |
| `kernel/src/mesh.cpp` | 编排 Plane + Sphere 的 CDT；Task 10 完成后移除耳切路径 |
| `cmake/BrepCore.cmake` | 将新 `.cpp` 编译进 `brep_core` |
| `tests/CMakeLists.txt` | 注册新测试可执行文件 |
| `tests/kernel/test_inner_loop.cpp` | 多 Outer 校验 |
| `tests/kernel/test_cdt.cpp` | CDT 单元测试 |
| `tests/kernel/test_loop_sample.cpp` | 采样 + 缝 |
| `tests/kernel/test_tessellate_inner.cpp` | 贴角孔 |
| `tests/kernel/test_tessellate_trimmed_sphere.cpp` | 裁剪球面 |
| `tests/kernel/test_tessellate_untitled_union.cpp` | `untitled.xl` 构型回归 |

---

### 任务 1：多 Outer 拓扑 + 校验

**文件：**
- 修改：`kernel/include/brep/topology.hpp`
- 修改：`kernel/src/topology.cpp`
- 修改：`kernel/src/validate.cpp`（Outer 数量检查约第 44–57 行）
- 修改：`tests/kernel/test_inner_loop.cpp`（`ExactlyOneOuterRequired` → 允许多个）
- 测试：`tests/kernel/test_inner_loop.cpp`

**接口：**
- 消费：现有 `Face::loops`、`LoopType`
- 产出：`std::vector<Loop*> Face::outer_loops() const;` — 所有 `type == Outer` 的环，稳定顺序 = `loops` 顺序。`outer_loop()` 仍为第一个 Outer 或 `nullptr`。

- [ ] **步骤 1：改写失败的校验期望**

将 `TEST(InnerLoop, ExactlyOneOuterRequired)` 替换为：

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

添加声明用法 — 在 `outer_loops` 存在前测试会编译失败；这可以接受 — 必要时先加仅编译桩。优先：先只改校验并让测试断言 `ok()`，再在同一任务中加 `outer_loops`。

同时保留：

```cpp
TEST(InnerLoop, MissingOuterStillErrors) {
  Model model;
  Body* body = make_planar_face_with_hole(model);
  Face* face = body->shells[0]->faces[0];
  for (Loop* l : face->loops) l->type = LoopType::Inner;
  EXPECT_FALSE(validate_body(*body).ok());
}
```

- [ ] **步骤 2：运行测试，验证当前行为不符合新期望**

```powershell
$env:PATH = "C:\Qt6\Tools\mingw1310_64\bin;" + $env:PATH
Set-Location E:\brepkernelstudy
cmake --build cmake-build-mingw-debug --target brep_test_inner_loop -j 8
.\cmake-build-mingw-debug\bin\brep_test_inner_loop.exe --gtest_filter=InnerLoop.MultipleOutersAllowed
```

预期：FAIL（校验仍要求恰好一个 outer）和/或若缺少 `outer_loops` 则编译错误。

- [ ] **步骤 3：实现 `outer_loops` + 校验变更**

在 `topology.hpp` 的 `Face` 内：

```cpp
[[nodiscard]] std::vector<Loop*> outer_loops() const;
```

在 `topology.cpp`：

```cpp
std::vector<Loop*> Face::outer_loops() const {
  std::vector<Loop*> outers;
  for (Loop* l : loops) {
    if (l && l->type == LoopType::Outer) outers.push_back(l);
  }
  return outers;
}
```

在 `validate.cpp` 将 `outer_count != 1` 错误块替换为：

```cpp
if (outer_count == 0) {
  report.error(fn, "missing outer loop");
  continue;
}
// outer_count >= 1 is OK (multi-outer allowed)
```

完全删除旧的 `"exactly one outer loop required"` 分支。

- [ ] **步骤 4：运行测试**

```powershell
.\cmake-build-mingw-debug\bin\brep_test_inner_loop.exe
```

预期：全部 PASS。

- [ ] **步骤 5：提交**

```powershell
git add kernel/include/brep/topology.hpp kernel/src/topology.cpp kernel/src/validate.cpp tests/kernel/test_inner_loop.cpp
git commit -m "Allow multiple outer loops on a face."
```

---

### 任务 2：CDT 网格 + 无约束 Delaunay

**文件：**
- 新建：`kernel/include/brep/mesh/cdt.hpp`
- 新建：`kernel/src/mesh/cdt.cpp`
- 修改：`cmake/BrepCore.cmake`（添加 `mesh/cdt.cpp`）
- 新建：`tests/kernel/test_cdt.cpp`
- 修改：`tests/CMakeLists.txt`（注册 `brep_test_cdt`）

**接口：**
- 消费：`brep/math.hpp` 中的 `brep::Point2d`
- 产出：

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

- [ ] **步骤 1：编写失败单元测试（正方形 → 2 个三角形）**

`tests/kernel/test_cdt.cpp`：

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

在 `tests/CMakeLists.txt` 中注册（参照其他测试）：

```cmake
add_executable(brep_test_cdt kernel/test_cdt.cpp)
target_link_libraries(brep_test_cdt PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_cdt
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_cdt>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_cdt."
)
```

- [ ] **步骤 2：运行测试 — 预期链接/编译失败**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt -j 8
```

预期：FAIL（缺少符号/文件）。

- [ ] **步骤 3：最小 Bowyer–Watson 实现**

用上述 API 创建 `cdt.hpp`。

在 `cdt.cpp` 中至少实现：
1. 覆盖所有点（+ 边距）的包围超级三角形。
2. 插入每个输入点；定位包含该点的三角形（行走或线性扫描）；Bowyer–Watson 空腔；向该点重新三角化。
3. 移除仍接触超级顶点的任意三角形。
4. 本任务忽略 `constraints`（仅接受空约束；若非空，设 `ok=false` 并给出消息 — 下一任务补全）。

添加到 `cmake/BrepCore.cmake`：

```cmake
${BREP_KERNEL_DIR}/src/mesh/cdt.cpp
```

- [ ] **步骤 4：运行测试 — 预期 PASS**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt -j 8
.\cmake-build-mingw-debug\bin\brep_test_cdt.exe --gtest_filter=Cdt.UnconstrainedSquareTwoTriangles
```

预期：PASS。

- [ ] **步骤 5：提交**

```powershell
git add kernel/include/brep/mesh/cdt.hpp kernel/src/mesh/cdt.cpp cmake/BrepCore.cmake tests/kernel/test_cdt.cpp tests/CMakeLists.txt
git commit -m "Add parametric CDT scaffold with unconstrained Delaunay."
```

---

### 任务 3：约束边 + 带孔多边形

**文件：**
- 修改：`kernel/src/mesh/cdt.cpp`
- 修改：`tests/kernel/test_cdt.cpp`

**接口：**
- 消费：任务 2 的 `triangulate_constrained`
- 产出：同一 API；约束被满足；调用方传入 outer+hole 边并过滤后，三角形仅在 outer 内 — **本任务**中，CDT 返回带约束边的点集完整 Delaunay；添加辅助函数：

```cpp
[[nodiscard]] CdtResult triangulate_polygon_with_holes(
    const std::vector<Point2d>& outer_ccw,
    const std::vector<std::vector<Point2d>>& holes_cw,
    double eps = 1e-12);
```

实现：合并顶点，为每个环构建连续约束边（闭合 last→first），调用 `triangulate_constrained`，再丢弃质心不满足 `in_outer && !in_any_hole` 的三角形。

- [ ] **步骤 1：失败测试 — 带方形孔的正方形**

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

在测试文件中完整实现重心坐标辅助函数（从 `test_tessellate_inner.cpp` 复制模式）。

- [ ] **步骤 2：运行 — 预期 FAIL**

```powershell
.\cmake-build-mingw-debug\bin\brep_test_cdt.exe --gtest_filter=Cdt.SquareWithHoleKeepsBoundaryAndDropsInterior
```

预期：FAIL（`ok=false` 或孔被覆盖）。

- [ ] **步骤 3：实现约束恢复 + 多边形辅助函数**

在 `cdt.cpp` 中：
- 对每条约束 `(i,j)`：当线段不在网格中时，找相交边；若可翻转且翻转使端点更接近连通，则翻转；否则在约束上插入 Steiner 中点，分裂，继续。
- 标记约束边，恢复后翻转不得破坏它们。
- 实现 `point_in_polygon`（射线法）用于过滤。
- 在 `cdt.hpp` 中暴露 `triangulate_polygon_with_holes`。

- [ ] **步骤 4：运行 — 预期 PASS**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt -j 8
.\cmake-build-mingw-debug\bin\brep_test_cdt.exe
```

预期：全部 PASS。

- [ ] **步骤 5：提交**

```powershell
git add kernel/include/brep/mesh/cdt.hpp kernel/src/mesh/cdt.cpp tests/kernel/test_cdt.cpp
git commit -m "Recover CDT constraints and triangulate polygons with holes."
```

---

### 任务 4：环边采样（直线 + 圆）

**文件：**
- 新建：`kernel/include/brep/mesh/loop_sample.hpp`
- 新建：`kernel/src/mesh/loop_sample.cpp`
- 修改：`cmake/BrepCore.cmake`
- 新建：`tests/kernel/test_loop_sample.cpp`
- 修改：`tests/CMakeLists.txt`

**接口：**
- 消费：`Loop`、`Edge`、`Curve`、`TessellationOptions`、`Surface`（`PlaneSurface` / `SphereSurface` 的 `param_of`）
- 产出：

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

采样规则：
- `LineCurve`：至少端点；若长度相对 `linear_deflection` 较大，均匀细分使直线弦高误差为 0（仅用端点也可）。
- `CircleCurve`：取 `n = max(2, ceil(Δθ / α))`，其中 `α` 来自角偏差及 `2*acos(1 - h/R)`，`h = linear_deflection`（回退 `0.02*R`）。
- 用 `PlaneSurface::param_of` 或 `SphereSurface::param_of` 将各 XYZ 投影。

- [ ] **步骤 1：失败测试 — 四分之一圆得到 >2 个采样点**

```cpp
TEST(LoopSample, QuarterCircleHasInteriorSamples) {
  Model model;
  // Build a single open-ish loop: center C, arc from +X to +Y on unit circle in XY
  // (minimal Face+Loop with one CircleCurve edge + two radii optional).
  // Assert sample_edge_xyz on the arc coedge has size() >= 4 for default opts.
}
```

用 `model.make_circle`、`make_edge`（`t0=0`、`t1=pi/2`），顶点在 `(1,0,0)` 与 `(0,1,0)` 构造。

- [ ] **步骤 2：运行 — 预期 FAIL**

- [ ] **步骤 3：实现采样**

接入 `sample_edge_xyz` / `sample_loop`。球面 `param_of` 将 `u` 归一化到 `[0, 2π)`。

- [ ] **步骤 4：PASS + 注册 `brep_test_loop_sample`**

- [ ] **步骤 5：提交**

```powershell
git commit -m "Add deflection-based loop edge sampling for tessellation."
```

---

### 任务 5：区域分组 + 平面 CDT 细分

**文件：**
- 修改：`kernel/src/mesh/loop_sample.cpp` / `.hpp`（添加 `group_face_regions`）
- 修改：`kernel/src/mesh.cpp`（`tessellate_plane_face` → CDT 路径）
- 测试：`tests/kernel/test_tessellate_inner.cpp`（现有测试须保持通过）

**接口：**
- 消费：`Face::outer_loops()`、`inner_loops()`、`sample_loop`、`triangulate_polygon_with_holes`
- 产出：

```cpp
struct FaceRegion {
  SampledRing outer;
  std::vector<SampledRing> holes;
};
[[nodiscard]] std::vector<FaceRegion> group_face_regions(
    const Face& face, const Surface& surface, const TessellationOptions& opts);
```

归属：孔质心 UV → `point_in_polygon(outer)`；若无匹配，`BREP_WARN` 并跳过该孔。

`tessellate_plane_face`：
1. `group_face_regions`
2. 对每个区域，`triangulate_polygon_with_holes`
3. 追加带平面法向 + 归一化 UV 的 3D 顶点
4. 尊重面朝向（现有翻转逻辑）

- [ ] **步骤 1：确保现有孔测试仍表达意图**

若已存在则无需修改；运行：

```powershell
.\cmake-build-mingw-debug\bin\brep_test_tessellate_inner.exe
```

切换实现后仍须 PASS。若在通过前切换，修复 CDT 过滤。

- [ ] **步骤 2：临时调用空 CDT 制造失败 — 可选；优先直接替换并观察 `HoleCenterNotCovered`。**

- [ ] **步骤 3：替换 `tessellate_plane_face` 主体** 为 CDT。暂时保留文件中旧 `bridge_hole` / `ear_clip` 函数（Task 10 删除）。

- [ ] **步骤 4：运行**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_tessellate_inner -j 8
.\cmake-build-mingw-debug\bin\brep_test_tessellate_inner.exe
```

预期：PASS（`HoleCenterNotCovered`）。

- [ ] **步骤 5：提交**

```powershell
git commit -m "Tessellate planar faces with parametric CDT."
```

---

### 任务 6：贴角孔（Sphere∪Box 平面情形）

**文件：**
- 修改：`tests/kernel/test_tessellate_inner.cpp`
- 修改：`kernel/src/mesh/cdt.cpp` / `loop_sample.cpp`（若需共享顶点合并）

**接口：**
- 消费：任务 5 的平面细分
- 产出：Inner 与 Outer 共享角顶点时网格正确

- [ ] **步骤 1：失败测试**

构造 sheet 面：Outer 单位正方形 `[0,1]²`；Inner 三角形 `(0,0) → (0.5,0) → (0,0.5)`（CW），在角 `(0,0)` 与 Outer 共享 — outer 与 inner 在该角使用同一 `Vertex*`。

```cpp
TEST(TessellateInner, CornerTouchingHoleNotCovered) {
  // ... build topology ...
  const TriangleMesh mesh = tessellate_body(*body);
  const Point3d inside_hole{0.15, 0.15, 0};
  const Point3d outside{0.8, 0.8, 0};
  // assert hole point not in any triangle; outside is
}
```

- [ ] **步骤 2：运行 — 若 CDT 重复角 UV 未合并或过滤错误，预期 FAIL**

- [ ] **步骤 3：修复** — 构建 CDT 输入时，在 `eps` 内（如相对 bbox 的 `1e-9`）合并 UV 点；不插入 bridge 边。

- [ ] **步骤 4：PASS**

- [ ] **步骤 5：提交**

```powershell
git commit -m "Support corner-touching inner loops in planar CDT tessellation."
```

---

### 任务 7：球面 UV 缝展开

**文件：**
- 修改：`kernel/include/brep/mesh/loop_sample.hpp`
- 修改：`kernel/src/mesh/loop_sample.cpp`
- 修改：`tests/kernel/test_loop_sample.cpp`

**接口：**
- 产出：

```cpp
/// Make ring UV contiguous: if |Δu|>π between adjacent samples, shift by ±2π
/// so the polyline does not jump the seam. May expand u outside [0,2π).
[[nodiscard]] SampledRing unwrap_sphere_ring(SampledRing ring);
```

- [ ] **步骤 1：失败测试**

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

调整 fixture 为三个明确跨缝的点。

- [ ] **步骤 2：FAIL**

- [ ] **步骤 3：实现顺序展开** — 对 `i=1..n-1`，当 `u[i]-u[i-1] > π` 减 `2π`；当 `< -π` 加 `2π`。闭合环时谨慎比较 last 与 first。

- [ ] **步骤 4：PASS**

- [ ] **步骤 5：提交**

```powershell
git commit -m "Unwrap sphere loop UV across the periodic seam."
```

---

### 任务 8：裁剪球面细分

**文件：**
- 修改：`kernel/src/mesh.cpp` — 当存在 `outer_loop()` 时，用 CDT 路径替换 `tessellate_sphere_face` 整网格
- 新建：`tests/kernel/test_tessellate_trimmed_sphere.cpp`
- 修改：`tests/CMakeLists.txt`
- 保持完整闭合球（`make_sphere` 单面带缝）可用：若唯一 outer 采样覆盖全参数域，CDT 仍填满球面；或在环为标准缝+极点时检测闭合解析球（现有路径）。**本任务规则：** 始终从采样环使用 CDT；若计数变化，更新 `brep_test_tessellate_sphere`，但法向/覆盖必须仍有效。

**接口：**
- 消费：`group_face_regions` + `unwrap_sphere_ring` + `triangulate_polygon_with_holes`
- 对每个 UV 顶点：`xyz = sphere.eval(u_mod, v)`，`u_mod = fmod(u, 2π)` 归一化到 `[0,2π)`；法向经 `face.normal_at`

- [ ] **步骤 1：失败测试 — ⅞ 球 outer 在删除的八分象限内无点**

复用 `build_axis_octant_ball` 路径的八分拓扑：调用布尔 Subtract 原点处 Sphere−Box，或构造相同 4 面体；细分；在球面上被移除 +++ 八分象限深处取点；断言未被覆盖。另：

```cpp
EXPECT_LT(mesh.vertices.size(), 300u);  // full default sphere is ~325
```

测量后调整阈值。

- [ ] **步骤 2：FAIL**（当前代码画整球 ⇒ 点被覆盖 / 顶点数高）

- [ ] **步骤 3：实现裁剪球面细分**

对有环的面删除/停止调用 nu×nv 网格。闭合 `make_sphere` 仍有绕缝的一个 outer 环 — 采样须覆盖整球参数域以便 CDT 填满（可能需要两极 + 密赤道）。若闭合球质量回退，在 UV 域内添加通过 inside-outer 测试的 Steiner 网格点（可选：用 `TessellationOptions` 段数作为 UV 格点，由 outer 裁剪）。

- [ ] **步骤 4：运行**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_tessellate_trimmed_sphere brep_test_tessellate_sphere -j 8
.\cmake-build-mingw-debug\bin\brep_test_tessellate_trimmed_sphere.exe
.\cmake-build-mingw-debug\bin\brep_test_tessellate_sphere.exe
```

预期：两者均 PASS。

- [ ] **步骤 5：提交**

```powershell
git commit -m "Tessellate trimmed sphere faces with seam-aware CDT."
```

---

### 任务 9：`untitled.xl` Sphere∪Box 细分回归

**文件：**
- 新建：`tests/kernel/test_tessellate_untitled_union.cpp`
- 修改：`tests/CMakeLists.txt`

**接口：**
- 消费：`evaluate` 布尔 Union + `tessellate_body`

- [ ] **步骤 1：带精确位姿的失败/回归测试**

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

将 inward 方向精化以匹配该角点的 `detect_corner_octant` 符号（`sx=-1,sy=-1,sz=+1`）。

- [ ] **步骤 2：对当前 main 运行 — 若任务 5–8 已完成，可能已 PASS；否则 FAIL 记录 bug**

- [ ] **步骤 3：修复剩余过滤/采样缺口直至 PASS**

- [ ] **步骤 4：同时运行曲面布尔 + 细分套件**

```powershell
.\cmake-build-mingw-debug\bin\brep_test_sphere_curved_boolean.exe --gtest_filter=SphereBoxBoolean.*
.\cmake-build-mingw-debug\bin\brep_test_tessellate_untitled_union.exe
```

- [ ] **步骤 5：提交**

```powershell
git commit -m "Add untitled.xl sphere-box union tessellation regression."
```

---

### 任务 10：移除耳切路径 + 文档交叉引用

**文件：**
- 修改：`kernel/src/mesh.cpp` — 删除未使用的 `bridge_hole`、`ear_clip_triangulate`、`ensure_ccw/cw`（若仅旧路径使用；仍需要的辅助函数保留）
- 修改：`docs/superpowers/specs/2026-08-10-analytic-sphere-brep-boolean-design.md` — 在 Phase 1 / 细分下简短注明指向 CDT 规格
- 修改：`docs/superpowers/specs/2026-08-11-trimmed-face-cdt-tessellation-design.md` — 状态 → 已实现（日期）

- [ ] **步骤 1：确认无测试调用旧符号**（grep `bridge_hole`）

- [ ] **步骤 2：删除死代码；构建所有网格相关测试**

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_cdt brep_test_loop_sample brep_test_tessellate_inner brep_test_tessellate_sphere brep_test_tessellate_trimmed_sphere brep_test_tessellate_untitled_union brep_test_inner_loop -j 8
```

- [ ] **步骤 3：运行上述全部 exe — 全部 PASS**

- [ ] **步骤 4：手动 viewer 检查（人工）：** 打开 `C:/Users/xingbl/Desktop/untitled.xl`，对两 GUID Fuse；预期实心盒 + 球冠。

- [ ] **步骤 5：提交**

```powershell
git commit -m "Remove ear-clip tessellation path; link CDT design as implemented."
```

---

## 规格覆盖清单

| 规格要求 | 任务 |
|------------------|------|
| 多 Outer 校验 + `outer_loops()` | 1 |
| CDT 数据结构 + 三角剖分 | 2–3 |
| 偏差环采样 | 4 |
| 区域分组 / 平面多孔 | 5 |
| 贴角孔 | 6 |
| 球面缝展开 | 7 |
| 裁剪球面（非整球） | 8 |
| `untitled.xl` 回归 | 9 |
| 无第三方库 / 移除耳切 | 10 + 全局约束 |
| 圆柱 | 明确延后（非目标） |

## 占位符 / 一致性自检

- 算法锁定为 Bowyer–Watson + 约束恢复（无 TBD）。
- API 名称一致：`triangulate_constrained`、`triangulate_polygon_with_holes`、`sample_loop`、`group_face_regions`、`unwrap_sphere_ring`。
- 测试可执行文件名与 `tests/CMakeLists.txt` 模式一致。
- 构建目录与 PATH 符合仓库惯例。
