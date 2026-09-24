# ADR 0008: 布尔运算仅保留通用 Pipeline（移除 AnalyticPair 特解）

**状态**：已接受  
**日期**：2026-08-24  
**修订**：[ADR 0006](0006-boolean-pipeline-architecture.md) 第 2 条（特解可选）  
**前置**：[ADR 0005](0005-boolean-backend-self-hosted.md)、[ADR 0006](0006-boolean-pipeline-architecture.md)、[通用 Pipeline 设计](../../superpowers/specs/2026-08-24-boolean-general-pipeline-only-design.md)

## 背景

Phase 2～3 以 **体类型对特解**（`BoxBoolean`、`SphereBoxBoolean`、`SphereSphereBoolean`、`PlanarBoolean`）快速验证了 B-Rep 布尔与 Feature 链路。ADR 0006 已将特解下沉为 `IAnalyticFastPath`，并规划通用 `BooleanPipeline` 为主路径。

当前问题：

- 每增加一种基元（圆柱、圆锥等）会触发 **O(n²) 体对体** 实现爆炸；
- 特解与 Pipeline **双轨维护**，Box−Sphere / Sphere−Box 对称性仍依赖 `CanHandle` 特例；
- **布尔结果再布尔**（一般 B-Rep 操作数）无法走特解；
- Pipeline 的 Imprint / Build 尚未完成，生产路径仍依赖特解。

团队决策：**不再扩展 AnalyticPair 特解**；`IBooleanEvaluator` 唯一实现为完整接线的通用 Pipeline。

## 决策

1. **`MakeDefaultBooleanEvaluator()`** 仅返回基于 `BooleanPipeline` 的 evaluator；**不再**注册 `AnalyticFastPathRegistry`。
2. **删除**（Pipeline 里程碑达标后）：
   - `FastPath.cpp/.h`、`BoxBoolean`、`SphereBoxBoolean`、`SphereSphereBoolean`、`PlanarBoolean` 及对应 public 头。
3. **保留并升级**：
   - `Intersect*`（曲面类型对求交）、`IntersectorRegistry`、`Broadphase`、`FaceSelector`、`BooleanPipeline` 六阶段。
   - **`SolidClassifier`**：`ClassifyPointInBody` 为 Classify 阶段 **唯一** 入口；**禁止** `RecognizeAxisAlignedBox` / `RecognizeAnalyticSphere` 或 `ClassifyPointInBox/Sphere` 作为 Pipeline 加速/分支。
4. **扩展方式**：新形体 → 新 `SurfaceKind` + 曲面–曲面对 `Intersect*` + Imprint + **射线–该曲面求交**（Classify 内按 `Face.Surface->Kind()` 分发）；**禁止**新增 `XxxYyyBoolean.cpp` 体对体入口。
5. **`BooleanEvalMode::AnalyticPair`** 标记废弃；成功结果统一 `General`。
6. **`CompositeBooleanEvaluator`** 简化为仅持有 `BooleanPipeline`（或合并为 `PipelineBooleanEvaluator`），对外仍实现 `IBooleanEvaluator`。
7. **OCCT 可选后端**（若未来引入）仍为独立 `IBooleanEvaluator` 实现，不恢复体对体特解层。

## 理由

- 与用户期望一致：任意 B-Rep 操作数、对称 CSG、可扩展曲面库。
- 消除双轨维护成本；测试只验证一条路径。
- 现有 `intersect_*`、BVH、`FaceSelector` 可直接接入 Pipeline，特解代码无长期架构价值。

## 后果

- **做**：
  - 按 [实施计划](../../superpowers/plans/2026-08-24-boolean-general-pipeline-implementation.md) 分期交付 Imprint / Classify / Build；
  - M1 先打通 Box×Box；M2 盒↔球；M3 球×球与链式布尔；M4 删除特解源码。
- **迁移**：现有 `TestBoxBoolean` 等保留用例，改断言 `mode=General`；删除 FastPath 相关测试。
- **短期**：在 M1 完成前，默认 evaluator 可能软失败于 Imprint/Build（与删特解前行为需按里程碑切换）。
- **不做（本 ADR）**：NURBS 通用求交、Sheet Split 特征、性能级 grid CSG 回退。

## 修订触发

- Pipeline 全阶段稳定且 ctest 全绿后，执行 M4 物理删除特解文件；
- 若性能成为瓶颈，Classify 仅允许 **按 `SurfaceKind` 的射线–曲面求交** 优化（与 Intersect 注册表同构），**不得**恢复体级 `Recognize*` 或 `ClassifyPointInBox/Sphere` 分支；不得恢复独立体对体 evaluator 入口。
