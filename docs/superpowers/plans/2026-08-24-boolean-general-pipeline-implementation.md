# 布尔通用 Pipeline 实施方案

> **设计规格：** `docs/superpowers/specs/2026-08-24-boolean-general-pipeline-only-design.md`  
> **决策记录：** `docs/architecture/adr/0008-boolean-general-pipeline-only.md`  
> **前置 ADR：** `docs/architecture/adr/0006-boolean-pipeline-architecture.md`

**Goal：** 移除 AnalyticPair 体对体特解，`IBooleanEvaluator` 仅通过完整 `BooleanPipeline` 求值；支持任意 B-Rep 操作数与按曲面类型扩展。

**Architecture：** 六阶段 Pipeline + 新建 Imprint / SolidClassifier / BooleanBuilder / TopologyCopy；`Intersect*` 从 Probe 升级为几何产出。

**Tech Stack：** 现有 `brep_core` 内核、`FaceBvh`、`Intersect*`、`ValidateBody`、GoogleTest、`cmake-build-mingw-debug`。

---

## Global Constraints

- **M4 之前** 不得物理删除特解源文件，避免长时间 ctest 全红（先 Pipeline 达标，再删代码）。
- Kernel 不引入 OCCT；可选后端仍通过独立 `IBooleanEvaluator` 插件。
- 拓扑修改仅作用于 Preprocess 克隆的 WorkingBody，不破坏 Feature 历史中的原始体。
- 每 PR 合并后：新增/迁移测试通过；未迁移用例可暂标 `DISABLED_` 并注明里程碑。
- 遵循项目 C++ 规范（PascalCase 文件、4 空格 Allman、`m_` 成员）。

---

## 里程碑总览

| 里程碑 | 交付 | 验收 |
|--------|------|------|
| **M0** | Evaluator 单路径 + PipelineState 扩展 | 默认 factory 不注册 FastPath |
| **M1** | Box×Box 全 op 端到端 | `TestBoxBoolean` 全绿，`mode=General` |
| **M2** | Box↔Sphere | `TestSphereBoxBoolean` / `TestSphereCurvedBoolean` 迁移 |
| **M3** | Sphere×Sphere + 链式布尔 | 布尔结果再布尔用例 |
| **M4** | 删除特解源码 | 无 `FastPath`/`BoxBoolean`/…；ctest 全绿 |
| **M5** | 柱面等（按需） | 圆柱相关 `Intersect*` + Imprint |

---

## M0 — 骨架整理

### Task M0.1 — Evaluator 单路径

**Files：** `CompositeEvaluator.cpp/.h`、`Evaluator.h`

- [ ] `MakeDefaultBooleanEvaluator()` 返回 **空 FastPath 注册表** 或移除 FastPath 成员
- [ ] 文档注释：默认仅 Pipeline
- [ ] `TestBooleanPipeline`：删除或改写 `CompositeUsesBoxBoxFastPath` 类用例

### Task M0.2 — PipelineState 扩展

**Files：** `Pipeline.h`、`Pipeline.cpp`

- [ ] 添加 `WorkingBodyA/B`、`IntersectionGraph`（可先空 struct）、`Fragments` 占位
- [ ] Preprocess：校验 + 预留 clone 钩子（可先 no-op clone，直接指针指向原 Body）

### Task M0.3 — 文档与 ADR

- [x] ADR 0008、设计规格、本实施计划
- [ ] 更新 ADR 0006 顶部「部分被 0008 修订」说明
- [ ] 更新 `AGENTS.md` 布尔架构引用

**验收：** 编译通过；`brep_test_boolean_pipeline` 中 Stage 顺序测试仍绿。

---

## M1 — Box×Box 通用闭环

### Task M1.1 — IntersectionGraph + Plane–Plane 几何

**Files：** 新建 `IntersectionGraph.h/.cpp`；改 `IntersectorRegistry.h/.cpp`

- [ ] `IntersectionSegment`：`Point3d` 端点 + 来源面指针 + 曲线类型
- [ ] `IntersectorRegistry::Intersect(Face, Face) → vector<IntersectionSegment>`
- [ ] Plane–Plane：求交线 ∩ 两面边界裁剪
- [ ] **测试：** `brep_test_intersection_graph` — 两正交盒共有棱线段

### Task M1.2 — TopologyCopy

**Files：** 新建 `TopologyCopy.h/.cpp`

- [ ] `CopyFaceSubgraph(Model& dst, const Face& src) → Face*`
- [ ] 复制链：Face → Loop → CoEdge → Edge → Vertex → Curve/Surface/Point
- [ ] **测试：** copy 后 `ValidateBody` 通过

### Task M1.3 — ImprintEngine（平面）

**Files：** 新建 `ImprintEngine.h/.cpp`

- [ ] `SplitEdgeAt(Edge*, double t)`
- [ ] 平面 Face 上插入交线分割 Loop
- [ ] ImprintStage 调用 ImprintEngine，产出 `Fragments`
- [ ] **测试：** `brep_test_imprint_planar` — 两盒相交后边数增加

### Task M1.4 — SolidClassifier（纯通用）

**Files：** 新建 `SolidClassifier.h/.cpp`；改 `Pipeline.cpp` ClassifyStage

- [ ] `ClassifyPointInBody(const Body&, Point3d, eps) → SolidClass` — 射线奇偶 + ON 距离
- [ ] `RaySurfaceHit` 按 `SurfaceKind` 注册（Plane / Sphere / Cylinder；Nurbs stub → Unsupported）
- [ ] 可选：Face BVH 加速射线–面候选
- [ ] ClassifyStage：**删除** 全部 `Recognize*` / `ClassifyPointInBox/Sphere` 调用
- [ ] **测试：** `brep_test_solid_classifier` — 盒/球/柱由 B-Rep Face 构建，不用 Recognize 辅助

### Task M1.5 — BooleanBuilder + BuildStage

**Files：** 新建 `BooleanBuilder.h/.cpp`；改 `Pipeline.cpp` BuildStage

- [ ] 从 Selection 复制面、Subtract 翻转 B
- [ ] 缝 Shell、`ValidateBody`
- [ ] `BooleanResult.OutputBody` 赋值
- [ ] **测试：** Select 后 Build 单壳

### Task M1.6 — 端到端 Box×Box

**Files：** `Pipeline.cpp`；迁移 `tests/kernel/TestBoxBoolean.cpp`

- [ ] 六阶段串联成功路径
- [ ] Union / Intersect / Subtract / 不相交 / 空结果
- [ ] 断言 `BooleanEvalMode::General`
- [ ] Viewer 盒布尔冒烟（可选）

**验收：** `ctest -R BoxBoolean` 全绿。

---

## M2 — Box↔Sphere

### Task M2.1 — Plane–Sphere Imprint

- [ ] 圆在平面 Face 上 imprint
- [ ] 圆在球面 Face 上 pcurve + seam 处理（参考 2026-08-10 §T3.6）

### Task M2.2 — 迁移球盒测试

- [ ] `TestSphereBoxBoolean`、`TestSphereCurvedBoolean` → `General`
- [ ] ⅛ Intersect、⅞ Subtract、角点 Union

**验收：** `ctest -R "SphereBox|SphereCurved"` 全绿。

---

## M3 — Sphere×Sphere + 链式布尔

### Task M3.1 — Sphere–Sphere 双面 Imprint

- [ ] 交圆印到两球面；选片

### Task M3.2 — 链式布尔

- [ ] 新增 `TestBooleanChained`：`(Box∩Sphere) − Box` 等
- [ ] 操作数为非基元 B-Rep

**验收：** 球×球 Union/Intersect；链式用例 `ValidateBody` 通过。

---

## M4 — 删除特解

### Task M4.1 — 物理删除

**删除文件：**

- `kernel/include/brep/bool/FastPath.h`
- `kernel/src/bool/FastPath.cpp`
- `BoxBoolean.h/.cpp`
- `SphereBoxBoolean.h/.cpp`
- `SphereSphereBoolean.h/.cpp`
- `PlanarBoolean.h/.cpp`

**修改：**

- [ ] `Boolean.h` — 移除特解 include
- [ ] `CompositeEvaluator` — 移除 FastPath 依赖，可选 rename
- [ ] `kernel/CMakeLists.txt` — 更新源文件列表
- [ ] `BooleanEvalMode::AnalyticPair` — `[[deprecated]]` 或删除

### Task M4.2 — 测试清理

- [ ] 删除 FastPath 专用测试
- [ ] `ctest -R boolean` 全绿

**验收：** 仓库内无 `EvaluateBoxBoolean` / `IAnalyticFastPath` 引用。

---

## M5 — 柱面扩展（按需）

- [ ] `IntersectCylinderCylinder`
- [ ] 柱面 Imprint
- [ ] 圆柱–盒 / 圆柱–球测试

---

## PR 顺序（推荐）

| PR | 内容 | 依赖 |
|----|------|------|
| PR-1 | M0.1 + M0.2 Evaluator 单路径、PipelineState | — |
| PR-2 | M1.1 IntersectionGraph + Plane–Plane | PR-1 |
| PR-3 | M1.2 TopologyCopy | PR-1 |
| PR-4 | M1.3 ImprintEngine 平面 | PR-2 |
| PR-5 | M1.4 SolidClassifier | PR-1 |
| PR-6 | M1.5 + M1.6 Build + Box×Box E2E | PR-3,4,5 |
| PR-7 | M2 球盒 | PR-6 |
| PR-8 | M3 球球 + 链式 | PR-7 |
| PR-9 | M4 删特解 | PR-8 |

---

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| Imprint 工作量大 | M1 仅平面；弯曲分 M2/M3 |
| 球面 seam/极点 | 独立 `SphereImprint` 子模块；参考 T3.6 笔记 |
| 删特解后回归 | M4 必须在 M1–M3 ctest 全绿之后 |
| 性能回退 | 可接受；后续仅优化 Stage 内部，不恢复体对体 API |

---

## 验证命令

```powershell
cmake --build cmake-build-mingw-debug --target brep_core
ctest -C Debug -R "boolean|BoxBoolean|SphereBox|SphereCurved|intersection|imprint|classifier" --output-on-failure
```

---

## 文档索引

| 文档 | 路径 |
|------|------|
| ADR 0008 | `docs/architecture/adr/0008-boolean-general-pipeline-only.md` |
| 设计规格 | `docs/superpowers/specs/2026-08-24-boolean-general-pipeline-only-design.md` |
| 原 Pipeline 规格 | `docs/superpowers/specs/2026-08-14-boolean-pipeline-design.md` |
| 原 ADR 0006 | `docs/architecture/adr/0006-boolean-pipeline-architecture.md` |
