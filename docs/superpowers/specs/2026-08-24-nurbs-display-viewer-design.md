# NURBS 曲线/曲面显示：Viewer 设计规格

**日期**：2026-08-24  
**状态**：已接受  
**总纲**：[NURBS 综合方案（Best-of-breed）](./2026-08-24-nurbs-unified-strategy.md)  
**决策**：[ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)  
**调研**：[CAD 功能调研](./2026-08-24-nurbs-display-cad-survey.md)  
**关联**：[CDT 裁剪面细分](./2026-08-11-trimmed-face-cdt-tessellation-design.md)、[Mesh.h](../../../kernel/include/brep/Mesh.h)

---

## 1. 目标与非目标

### 目标

- 在 Viewer 中 **正确显示** NURBS 曲线与 NURBS 裁剪面（Shaded + Wireframe）。
- 与 Plane / Sphere / Cylinder 共用同一套 `TessellateBody` / `ExtractEdges` 入口。
- 显示质量可通过 `TessellationOptions` 调节（弦高、角度、段数上下限）；**目标形态**见 [综合方案 §4.1](./2026-08-24-nurbs-unified-strategy.md) `NurbsDisplayQuality`。
- 可选：等参线、控制网（建模/debug 模式）。

### 非目标（本规格）

- NURBS 求交、布尔、Imprint。
- 交互式 CV 编辑 → 见 [建模 UI 远期路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md) P2
- NURBS 导入/导出（STEP/IGES）；可后续单独立项。
- 曲率分析、斑马线 G0/G1 检查。

---

## 2. 现状

| 项 | 状态 |
|----|------|
| `SurfaceKind::Nurbs` / `CurveKind::Nurbs` | `Types.h` 已枚举 |
| `NurbsCurve` / `NurbsSurface` 类 | **未实现** |
| `Mesh.cpp` tessellation | Plane（CDT）、Sphere（UV+CDT）；**无 Nurbs 分支** |
| `TessellationOptions` | `LinearDeflection`、`AngularDeflection`、UV segment 上下限 |
| Viewer | Vulkan 消费 `TriangleMesh` + `EdgeMesh` |

---

## 3. 架构

```text
Model (NurbsSurface / NurbsCurve)
        │
        ▼
  Surface::Eval(u,v) / Curve::Eval(t)
        │
        ├─► TessellateFace ──► TriangleMesh (shaded)
        ├─► ExtractEdges   ──► EdgeMesh (wireframe)
        └─► ExtractIsoLines (optional) ──► EdgeMesh
        │
        ▼
  SceneAdapter / BksCache
        │
        ▼
  Vulkan Viewer
```

**原则**：精确 NURBS 留在 `Model`；Viewer 只接收 mesh。

---

## 4. 几何表示（Kernel）

### 4.1 NurbsCurve

建议最小字段（非有理 B-spline 可先落地，有理后续加权重）：

| 字段 | 说明 |
|------|------|
| `Degree` | 阶数 p |
| `Knots` | 节点向量（clamped uniform 或 general） |
| `ControlPoints` | 3D 控制点；有理时带权重 w |
| `Domain` | 有效参数区间 [t0, t1] |

API：

- `Point3d Eval(double t) const`
- `Vector3d Tangent(double t) const`（显示法线/管状 mesh 用）
- `CurveKind Kind() const noexcept { return CurveKind::Nurbs; }`

### 4.2 NurbsSurface

| 字段 | 说明 |
|------|------|
| `DegreeU`, `DegreeV` | U/V 阶数 |
| `KnotsU`, `KnotsV` | 节点向量 |
| `ControlPoints` | (n+1)×(m+1) 控制网；有理时权重 |
| `DomainU`, `DomainV` | 有效 UV 区间 |

API：

- `Point3d Eval(double u, double v) const`
- `Vector3d Normal(double u, double v) const`（偏导叉积）
- `SurfaceKind Kind() const noexcept { return SurfaceKind::Nurbs; }`

### 4.3 Model 工厂

```cpp
NurbsCurve* MakeNurbsCurve(...);
NurbsSurface* MakeNurbsSurface(...);
```

与 `MakePlane` / `MakeSphereSurface` 同级，归 `Model` 池所有。

---

## 5. Tessellation 策略

对齐 [CAD 调研 §3](./2026-08-24-nurbs-display-cad-survey.md) 与现有 `TessellationOptions`。

### 5.1 NURBS 曲线 → EdgeMesh

1. 在参数域 `[t0, t1]` 自适应采样：
   - **弦高约束**：采样折线到真实曲线的最大距离 ≤ `LinearDeflection`（或默认 `max(1e-4, chord)`）。
   - **角度约束**：相邻段方向夹角 ≤ `AngularDeflection`。
2. 输出 `EdgeMesh` 折线段（与 `ExtractEdges` 中线性 Edge 一致）。

### 5.2 NURBS 曲面 → TriangleMesh（未裁剪）

**Phase A — Uniform UV 网格（实现简单，用于调试）**

- 在 `[u0,u1]×[v0,v1]` 均匀分格；段数 clamp 到 `Min/MaxUSegments`、`Min/MaxVSegments`。

**Phase B — Adaptive tessellation（目标，对齐 CATIA/SW）**

1. 从粗 UV 网格开始。
2. 细分直到满足：
   - 三角边中点到曲面弦高 ≤ `LinearDeflection`；
   - 相邻三角法向夹角 ≤ `AngularDeflection`。
3. 法向：解析偏导（优于顶点平均）。

### 5.3 NURBS 裁剪面（Face 带 Loop）

与 Plane/Sphere 相同：**参数域 CDT**

1. 将 Loop 边（Line / Circle / **Nurbs**）采样为 2D pcurve 或 3D 边再投影到曲面 UV。
2. 在 `(u,v)` 平面做约束 Delaunay（现有 CDT 管线）。
3. 三角中心映射 `Surface::Eval` 回 3D。

> 若 pcurve 未就绪，M1 可限制为 **NURBS 张量积面 + 直线/圆弧边界**；全 NURBS 边界放 M2。

### 5.4 等参线（可选）

新增 `IsoLineOptions`：

```cpp
struct IsoLineOptions {
  int CountU{4};
  int CountV{4};
};
```

`ExtractIsoLines(const Face&, EdgeMesh& out, const IsoLineOptions&)` — 在曲面 U/V 常数线采样，参考 MicroStation rule lines。

### 5.5 控制网（可选）

`ExtractControlNet(const NurbsSurface&) → EdgeMesh`（控制点网格线）；默认关闭，Debug 菜单开启。

---

## 6. TessellationOptions 扩展（建议）

| 字段 | 说明 | 默认 |
|------|------|------|
| `LinearDeflection` | 弦高 / sag | 0 → 按包围盒比例自动 |
| `AngularDeflection` | 法向最大夹角（rad） | 15° |
| `MinUSegments` / `MaxUSegments` | UV 段数上下限 | 现有值 |
| `MinVSegments` / `MaxVSegments` | 同上 | 现有值 |
| `ProportionalToBBox` | sag ∝ 包围盒对角线（CATIA 思路） | `true`（新增） |
| `ViewScaleRefine` | 按相机距离增密（NX View 思路） | P1 |

---

## 7. Viewer 集成

### 7.1 数据流

无变更：`Document` → `SceneAdapter::SyncBody` → `TessellateBody` → GPU buffer。

NURBS Body 与 Box/Sphere 相同路径；`.bks` 缓存 key 需含 `TessellationOptions` 哈希。

### 7.2 显示模式

| 模式 | NURBS 行为 |
|------|-----------|
| Shaded | `TriangleMesh` |
| Wireframe | `ExtractEdges` + 可选 `ExtractIsoLines` |
| Shaded + Edges | 两者叠加 |

### 7.3 设置 UI（P1）

- 质量预设：Draft / Standard / Fine（映射到 `TessellationOptions`）。
- 高级：弦高、角度（对标 SolidWorks Image Quality）。

---

## 8. 文件布局（目标）

```text
kernel/include/brep/
  Geometry.h              # NurbsCurve, NurbsSurface
  Mesh.h                  # IsoLineOptions, ExtractIsoLines (optional)
  NurbsEval.h             # de Boor / 曲面求值（可选独立头）

kernel/src/
  NurbsEval.cpp
  Mesh.cpp                # TessellateNurbsFace, adaptive 逻辑
  GeometryNurbs.cpp       # 或合入 Geometry 实现文件

tests/kernel/
  TestNurbsEval.cpp
  TestTessellateNurbs.cpp

apps/viewer/
  （P1）设置面板绑定 TessellationOptions
```

---

## 9. 测试策略

| 用例 | 断言 |
|------|------|
| `NurbsEval.LineSegment` | 2 点 1 次 B-spline 与直线一致 |
| `NurbsEval.CircleArc` | 有理圆近似误差 < tol |
| `TessellateNurbs.SpherePatch` | NURBS 球面片 vs 解析球 chord 误差 |
| `TessellateNurbs.TrimmedFace` | 带 Loop 的 NURBS 面 `ValidateBody` + 水密 mesh |
| `ExtractIsoLines.Count` | U/V 条数与选项一致 |

---

## 10. 与布尔 / 建模路线关系

```text
[本规格] NURBS Display (P0/P1)
        │
        ▼ (后续 ADR)
[ADR 0008] 通用布尔 Pipeline M1–M4（解析面）
        │
        ▼ (更后)
NURBS 求交 + Imprint
```

**禁止**：为 NURBS 显示单独开 `XxxYyyBoolean` 特解。

---

## 11. 参考

- [The NURBS Book](https://www.springer.com/)（Piegl & Tiller）— de Boor、有理形式
- 现有 CDT：[2026-08-11 设计](./2026-08-11-trimmed-face-cdt-tessellation-design.md)
- CAD 调研：[2026-08-24-nurbs-display-cad-survey.md](./2026-08-24-nurbs-display-cad-survey.md)
