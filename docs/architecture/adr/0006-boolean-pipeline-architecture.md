# ADR 0006: 布尔运算通用 Pipeline 架构

**状态**：已接受  
**日期**：2026-08-14  
**前置**：[ADR 0005](0005-boolean-backend-self-hosted.md)、[2026-08-10 布尔设计](../../superpowers/specs/2026-08-10-analytic-sphere-brep-boolean-design.md) §B3、[2026-08-14 Pipeline 规格](../../superpowers/specs/2026-08-14-boolean-pipeline-design.md)

## 背景

Phase 2～3 以 **AnalyticPair 特解**（盒–盒、球–盒角点、球–球等）推进验证，但 `DefaultBooleanEvaluator` 已演化为按类型组合的 `if-else` 路由。该方式：

- 无法自然支持 **Box−Sphere** 与 **Sphere−Box** 对称；
- 每增加一种曲面/体组合就要新增 `evaluate_*_*` 入口；
- 与设计文档 §B3「求交 → 印记 → 分类 → 选面 → 重建」通用流水线不一致。

宽相 BVH 与 `intersect_*` 解析求交已存在，**印记/分类/重建尚未接线**（`broadphase` 诊断中已注明）。

## 决策

1. **主路径**改为可阶段化的 `BooleanPipeline`（Preprocess → Intersect → Imprint → Classify → Select → Build）。
2. **AnalyticPair 特解**下沉为 `IAnalyticFastPath` 插件，由 `CompositeBooleanEvaluator` 调度：`can_handle()==false` 时走 Pipeline；`can_handle()==true` 时特解结果（成功或失败）直接返回。
3. **面–面求交**按 **曲面类型对** 注册到 `IntersectorRegistry`（Plane–Sphere 等），**不按体类型对**（Box–Sphere）注册。
4. **CSG 选面**与几何类型解耦：`select_faces(op, A, B, classifications)` 只依赖 IN/OUT/ON 与 `BooleanOp`。
5. **体被曲面裁剪**（Split / Trim）不在本 ADR 范围；后续可复用 Intersect + Imprint 阶段，特征层单独 `SplitFeature`。
6. 保留 `IBooleanEvaluator` 对外接口；`make_default_boolean_evaluator()` 返回 `CompositeBooleanEvaluator`。

## 理由

- 与用户期望一致：两种差集方向、未来更多类型，应共享一套 CSG 规则与拓扑流水线。
- 特解保留为 **性能/精度优化**，不绑架架构。
- 现有 `intersect_*`、`classify_*`、`FaceBvh` 可逐步接入各 Stage，无需推翻 Phase 1～3 成果。

## 后果

- **做**：新增 `kernel/include/brep/bool/pipeline*.hpp` 等；第一版 Stage 可软失败并给出阶段诊断；P1 目标为 Box↔Sphere 双向走通用路径。
- **迁移**：`EvaluatorStub.cpp` 中路由逻辑迁至 `FastPath.cpp`；原 `evaluate_*` 函数保留为特解实现。
- **测试**：`brep_test_boolean_pipeline` 验证 Stage 顺序、Composite 回退、Box–Box 仍走特解。
- **不做（本 ADR）**：完整 Imprint/Build 实现、NURBS 求交、曲面-sheet 裁剪特征。

## 修订触发

- 通用 Pipeline 稳定后，评估是否将部分 AnalyticPair 标记为 deprecated；
- 若引入 OCCT 后端，实现为另一 `IBooleanEvaluator`，不修改 Pipeline 阶段契约。
