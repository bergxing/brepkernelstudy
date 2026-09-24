# ADR 0009: NURBS 显示范围（Display-only，与布尔解耦）

**状态**：已接受  
**日期**：2026-08-24  
**前置**：[ADR 0005](0005-boolean-backend-self-hosted.md)、[ADR 0008](0008-boolean-general-pipeline-only.md)、[CAD 调研](../../superpowers/specs/2026-08-24-nurbs-display-cad-survey.md)

## 背景

`SurfaceKind::Nurbs` / `CurveKind::Nurbs` 已在 `Types.h` 枚举中预留，但 `Geometry.h` / `Mesh.cpp` 尚未落地 NURBS 实体与 tessellation。团队计划引入 NURBS **绘制**，需明确与布尔、建模内核的边界，避免范围膨胀（见 [2026-08-10 布尔设计](../superpowers/specs/2026-08-10-analytic-sphere-brep-boolean-design.md) 风险表）。

主流 CAD 均采用 **精确 NURBS + 显示 tessellation 分离**（见 CAD 调研文档）。

## 决策

1. **首期目标为 Display-only**：NURBS 曲线/曲面的 **求值、tessellation、线框等参线、可选控制网**；供 Viewer 渲染与调试。
2. **不做（首期）**：NURBS–NURBS / NURBS–解析面 **求交**、NURBS 布尔 Imprint、CAM 刀路。
3. **几何存储**：在 `kernel/include/brep/Geometry.h` 增加 `NurbsCurve` / `NurbsSurface`（或等价 B-spline 表示），由 `Model` 工厂拥有；遵循现有 `PlaneSurface` / `SphereSurface` 模式。
4. **Tessellation**：扩展 `TessellateFace` / `ExtractEdges`；复用并扩展 `TessellationOptions`（弦高、角度、UV 段数上下限）；裁剪面仍走现有参数域 CDT 管线（[2026-08-11 CDT 设计](../superpowers/specs/2026-08-11-trimmed-face-cdt-tessellation-design.md)）。
5. **Viewer 边界**：Viewer 仅 `#include "api/Mesh.h"` 等；不直接依赖 NURBS 数学头。
6. **缓存**：显示 mesh 可进 `.bks` / Viewer 缓存；**禁止**将 tessellation 三角网写回精确 B-Rep 拓扑。
7. **布尔顺序**：NURBS 求交纳入布尔 Pipeline **更后阶段**（解析曲面 Imprint/Build 稳定之后）；本 ADR 不阻塞 [ADR 0008](0008-boolean-general-pipeline-only.md) 的 M1–M4。

## 理由

- 与行业实践一致：先能 **看**，再能 **算**（求交/布尔）。
- 降低与通用布尔 Pipeline 的并行冲突；避免「显示 + 求交 + 布尔」三轨同时铺开。
- 现有 `Mesh.h` / Vulkan Viewer 已有 Plane/Sphere tessellation 与 CDT，扩展点清晰。

## 后果

- **做**：按 [Viewer 设计规格](../superpowers/specs/2026-08-24-nurbs-display-viewer-design.md)、[综合方案](../superpowers/specs/2026-08-24-nurbs-unified-strategy.md) 与 [实施计划](../superpowers/plans/2026-08-24-nurbs-display-implementation.md) 分期交付。
- **不做**：NURBS 特征树、自由曲面建模 UI、第三方 NURBS 库整库引入（除非后续 ADR 修订）。
- **远期占位**：[NURBS 建模与自由曲面 UI 路线图](../superpowers/specs/2026-08-24-nurbs-modeling-ui-roadmap.md)（P1–P4，未排期）。
- **可选参考**：`third_party/eigen` Splines 仅作算法参考；优先 lightweight 自研或最小依赖。

## 修订触发

- Viewer NURBS 显示稳定后，再开 ADR 修订 NURBS 求交 / 布尔范围；
- NURBS 特征树 / 自由曲面 UI：见 [建模 UI 远期路线图](../superpowers/specs/2026-08-24-nurbs-modeling-ui-roadmap.md)，正式立项时新增 **ADR 0011**（**0010 已用于内核库拆分**）；
- 若引入 OCCT 仅作 tessellation/IO，需单独 ADR，不改变「显示 mesh 不写回 B-Rep」原则。
