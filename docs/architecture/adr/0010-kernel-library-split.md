# ADR 0010: 内核库按职责拆分（base / core / mesh / bool）

**状态**：已接受（设计）；落地见实施计划  
**日期**：2026-08-26  
**前置**：ADR 0002、0003、0004、0008  
**总图**：[layering.md](../layering.md)

## 背景

ADR 0003 已把内核拆成 `brep_core ← brep_feat ← brep_asm / brep_io`，Viewer 拆成 adapter / runtime / ui。  
`brep_core` 仍编译布尔 Pipeline、CDT、Guid、Log，改一处布尔实现会拖垮「几何内核」增量编译，也无法在不链接布尔的情况下只使用 tessellate。

Viewer 与 adapter **已经**使用 `brep::Guid`（ECS `BodyRef`）和 `BREP_*` 日志。这两件是进程级公共件，不是「仅内核几何」私有物。

## 决策

1. **同仓增加 CMake target**，不增加 Git 仓库：
   - `brep_base` — Math、Guid、Log
   - `brep_core` — 几何、拓扑、Model、Builder、Validate、IObject（变瘦）
   - `brep_mesh` — tessellate / CDT / LoopSample
   - `brep_bool` — 通用布尔 Pipeline（ADR 0008）与 `IBooleanEvaluator` 实现
2. **依赖 DAG（SHARED 无环）：**
   ```text
   brep_base ← brep_core ← brep_mesh ← brep_io
                         ← brep_bool ← brep_feat ← brep_asm
                                              ↖ brep_io（另链 asm + mesh：.bks tessellate）
   ```
3. **`Part` 留在 `brep_feat`**，继续注入 `IBooleanEvaluator`；实现放 `brep_bool`。禁止 Part 下沉 core（会环）。
4. **INTERFACE 目标 `brep` 继续聚合全部内核库**，Viewer / 现有测试可仍 `target_link_libraries(... brep)`。
5. **先改 CMake 源文件列表，后（可选）搬目录。** `SnapQuery` / `FaceBvh` 第一期留在 `brep_core`。
6. **公开 API：** 新增 `api/Base.h`；`api/Mesh.h` 归属改为 `brep_mesh`；`api/Modeling.h` 不再 include 整份 `brep/bool/Boolean.h` 伞头（实施阶段收紧）。
7. Viewer 分层维持 ADR 0003；**不**重拆 `viewer_runtime`；adapter 不得平行封装 Guid/日志类型。
8. **三种真相分开**：特征历史 ≠ 精确 B-Rep ≠ 显示 mesh。NURBS 存储在 core、显示 tessellate 在 `brep_mesh`、NURBS 求交（另 ADR）在 `brep_bool`。
9. 内核组合用工厂 + `IBooleanEvaluator`；Hypodermic 只注册工厂能提供的服务，不注册 Pipeline 内部类型（见 [layering.md](../layering.md) §5）。

## 理由

- 布尔与网格是独立变化轴，且体积已经大于 Builder/Validate。
- Guid/Log 被 UI 使用，从 core 提到 `brep_base` 后，语义与链接都诚实：上层依赖基础库，而不是「偷偷从几何 DLL 再导出」。
- 聚合 `brep` 保留，拆库不强迫 Viewer 一次改完链接行。

## 后果

- Windows 运行目录多 `brep_base.dll`、`brep_mesh.dll`、`brep_bool.dll`。
- `cmake/BrepInstall.cmake` 的 install 列表必须同步新 target。
- 类型归属文档 [`api-module-owners.md`](../api-module-owners.md) 在实施 Phase C 更新。
- 详细图与非目标：[layering.md](../layering.md)。
- NURBS **建模**立项使用 **ADR 0011**，不占用本号（修订 ADR 0009 中的旧建议编号）。

## 参考

- 设计：[layering.md](../layering.md)
- 步骤（详细）：[2026-08-26-kernel-library-split-implementation.md](../../superpowers/plans/2026-08-26-kernel-library-split-implementation.md)
