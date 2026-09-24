# NURBS 显示实施方案

> **设计规格：** `docs/superpowers/specs/2026-08-24-nurbs-display-viewer-design.md`  
> **综合总纲：** `docs/superpowers/specs/2026-08-24-nurbs-unified-strategy.md`  
> **CAD 调研：** `docs/superpowers/specs/2026-08-24-nurbs-display-cad-survey.md`  
> **决策记录：** `docs/architecture/adr/0009-nurbs-display-scope.md`

**Goal：** 在 Kernel + Viewer 中落地 NURBS 曲线/曲面 **显示**（tessellation + 线框），与解析面共用 `Mesh.h` 入口；不涉及 NURBS 布尔。

**Architecture：** `NurbsCurve` / `NurbsSurface` → 求值 → adaptive/uniform tessellation → 现有 Vulkan 路径。

**Tech Stack：** C++20、`Mesh.cpp` CDT 管线、GoogleTest、Qt6/Vulkan Viewer、`cmake-build-mingw-debug`。

---

## Global Constraints

- Display-only：不实现 NURBS `Intersect*` / Boolean Imprint。
- Tessellation **不写回** B-Rep；可缓存 `.bks`。
- Viewer 生产代码仅 `#include "api/..."`（ADR 0002）。
- 遵循 PascalCase 文件、4 空格 Allman、`m_` 成员。
- 每里程碑合并后：新增测试通过；不破坏现有 Plane/Sphere tessellation 测试。

---

## 里程碑总览

| 里程碑 | 交付 | 验收 |
|--------|------|------|
| **N0** | 文档 + ADR | 本计划与 ADR 0009（已完成） |
| **N1** | Nurbs 求值 + 曲线折线化 | `TestNurbsEval` 全绿 |
| **N2** | 未裁剪 NURBS 面 uniform tessellation | 球面片 mesh chord 误差达标 |
| **N3** | Adaptive tessellation（弦高+角度） | 与 N2 对比三角数/误差 |
| **N4** | 裁剪 NURBS 面 + CDT | `ValidateBody` + 显示正常 |
| **N5** | 等参线 + Viewer 质量预设（可选） | Wireframe 下 rule lines |
| **N6** | 控制网 debug 层（可选） | 开关生效 |

---

## N0 — 文档（已完成）

- [x] ADR 0009、CAD 调研、Viewer 设计、本实施计划
- [x] `AGENTS.md` 索引

---

## N1 — NURBS 求值与曲线显示

### Task N1.1 — 几何类型

**Files：** `Geometry.h`、`Model.h`、`Model.cpp`（或 `GeometryNurbs.cpp`）

- [ ] `NurbsCurve`：`Degree`、`Knots`、`ControlPoints`（+ 可选 `Weights`）
- [ ] `NurbsSurface`：`DegreeU/V`、`KnotsU/V`、控制网
- [ ] `Model::MakeNurbsCurve` / `MakeNurbsSurface`
- [ ] `Eval` / `Tangent` / `Normal`

### Task N1.2 — de Boor 实现

**Files：** 新建 `NurbsEval.h/.cpp`

- [ ] 曲线 de Boor（clamped knot）
- [ ] 曲面双方向 de Boor 或张量积展开
- [ ] 有理 NURBS（权重）— 可 N1.2b 子任务
- [ ] **测试：** `TestNurbsEval` — 直线、抛物线、圆近似

### Task N1.3 — 曲线 EdgeMesh

**Files：** `Mesh.cpp`、`Mesh.h`

- [ ] `Edge` 含 `NurbsCurve` 时 `ExtractEdges` 自适应折线化
- [ ] 弦高 + 角度停止条件
- [ ] **测试：** 折线数随 `LinearDeflection` 单调

**验收：** `ctest -R NurbsEval` 全绿；Viewer 可显示 NURBS 线框 Edge（若已有线 only body）。

---

## N2 — 未裁剪 NURBS 面 Uniform Tessellation

### Task N2.1 — Mesh 分支

**Files：** `Mesh.cpp`

- [ ] `TessellateFace` 增加 `SurfaceKind::Nurbs` → uniform UV 网格
- [ ] 段数 respect `Min/MaxUSegments`、`Min/MaxVSegments`
- [ ] 法向来自解析偏导

### Task N2.2 — 回归对比

- [ ] 用 NURBS 张量积面近似 **球面 octant**，与 `SphereSurface` tessellation 对比 chord 误差
- [ ] **测试：** `TestTessellateNurbs.UniformSpherePatch`

**验收：** Shaded 显示 NURBS 球面片无 obvious 棱边（Fine 预设）。

---

## N3 — Adaptive Tessellation

### Task N3.1 — 自适应细分

**Files：** `Mesh.cpp`

- [ ] 递归或队列细分 UV 四边形/三角 until chord + angle 满足
- [ ] `LinearDeflection == 0` 时按 bbox 比例默认 sag（`ProportionalToBBox`）
- [ ] **测试：** 同曲面 Fine vs Draft 三角数比 > 2

### Task N3.2 — BksCache 键

**Files：** `BksCache.cpp`（若需）

- [ ] 缓存 key 含 `TessellationOptions` 哈希

**验收：** 调节质量预设可见 mesh 密度变化。

---

## N4 — 裁剪 NURBS 面（CDT）

### Task N4.1 — Loop 采样

- [ ] NURBS 边界：`Edge` 为 Line/Circle/Nurbs 时在 UV 上生成约束边
- [ ] 接入现有参数域 CDT（[2026-08-11 计划](./2026-08-11-trimmed-face-cdt-tessellation.md)）

### Task N4.2 — 端到端 Body

- [ ] 构造 NURBS 面 + Outer Loop 的 Solid
- [ ] `TessellateBody` + `ValidateBody`
- [ ] **测试：** `TestTessellateNurbs.TrimmedFace`

**验收：** 带孔/贴边 NURBS 面显示无空洞（参考 CDT 球∪盒经验）。

---

## N5 — 等参线（可选）

### Task N5.1 — ExtractIsoLines

**Files：** `Mesh.h/.cpp`

- [ ] `IsoLineOptions` + `ExtractIsoLines`
- [ ] Viewer wireframe 模式叠加（SceneAdapter 或 render pass）

**验收：** MicroStation 式 U/V rule lines 可调密度。

---

## N6 — 控制网 Debug（可选）

- [ ] `ExtractControlNet` for curve/surface
- [ ] Viewer Debug 菜单 Toggle

---

## PR 顺序（推荐）

| PR | 内容 | 依赖 |
|----|------|------|
| PR-N1 | N1.1–N1.3 求值 + 曲线 mesh | — |
| PR-N2 | N2 uniform 曲面 | PR-N1 |
| PR-N3 | N3 adaptive + cache | PR-N2 |
| PR-N4 | N4 CDT 裁剪面 | PR-N3 |
| PR-N5 | N5 等参线 | PR-N2 |
| PR-N6 | N6 控制网 | PR-N1 |

N5/N6 可与 N3/N4 并行。

---

## 与布尔 Pipeline 并行策略

| 线 | 负责 | 冲突点 |
|----|------|--------|
| NURBS Display（本计划） | `Geometry` / `Mesh` | 无 |
| Boolean Pipeline M1–M4 | `bool/` | 无（ADR 0009 禁止 NURBS 布尔） |

可并行开发；合并时注意 `Mesh.cpp` 冲突，建议 NURBS tessellation 独立函数文件 `MeshNurbs.cpp`。

---

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 有理 NURBS 复杂 | N1 先非有理；圆用有理为 N1.2b |
| CDT + NURBS pcurve 未就绪 | N4 先直线/圆弧边界 |
| Adaptive 性能 | `MaxUSegments` cap；`.bks` 缓存 |
| `Mesh.cpp` 过大 | 拆 `MeshNurbs.cpp` |

---

## 验证命令

```powershell
cmake --build cmake-build-mingw-debug --target brep_core brep_test_nurbs_eval brep_test_tessellate_nurbs
ctest -C Debug -R "Nurbs|TessellateNurbs" --output-on-failure
```

---

## 文档索引

| 文档 | 路径 |
|------|------|
| ADR 0009 | `docs/architecture/adr/0009-nurbs-display-scope.md` |
| CAD 调研 | `docs/superpowers/specs/2026-08-24-nurbs-display-cad-survey.md` |
| Viewer 设计 | `docs/superpowers/specs/2026-08-24-nurbs-display-viewer-design.md` |
| 建模/UI 远期 | `docs/superpowers/specs/2026-08-24-nurbs-modeling-ui-roadmap.md` |
| **综合方案（总纲）** | `docs/superpowers/specs/2026-08-24-nurbs-unified-strategy.md` |
| CDT 细分 | `docs/superpowers/specs/2026-08-11-trimmed-face-cdt-tessellation-design.md` |
