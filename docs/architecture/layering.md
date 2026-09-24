# 工程分层（活文档）

**状态**：CMake 已按目标拆库；需本机重新 build 产出 DLL  
**日期**：2026-08-26  
**角色**：整仓分层的**唯一总图**。CMake 尚未按目标拆完 `brep_core` 时，以本文「目标」为准、以「现状」对照代码。

| 决策 | 文档 |
|------|------|
| SHARED DAG、Viewer 三库 | [ADR 0003](adr/0003-shared-libraries.md) |
| 同仓多子工程 | [ADR 0004](adr/0004-monorepo-subprojects.md) |
| `api/*` 边界 | [ADR 0002](adr/0002-api-header-tiers.md)、[api-module-owners.md](api-module-owners.md) |
| IoC 仅 Viewer | [ADR 0007](adr/0007-hypodermic-ioc-adoption.md)、[ioc-governance.md](ioc-governance.md) |
| 边界切面在 `brep_base`，内核用进程链，命令用窗口链 | [ADR 0012](adr/0012-viewer-command-aspects.md)、[切面规格](../superpowers/specs/2026-09-23-viewer-command-aspects-design.md) |
| 布尔仅通用 Pipeline | [ADR 0008](adr/0008-boolean-general-pipeline-only.md) |
| NURBS 先显示后求交 | [ADR 0009](adr/0009-nurbs-display-scope.md) |
| 内核再拆 base/mesh/bool | [ADR 0010](adr/0010-kernel-library-split.md) |
| 拆库步骤 | [2026-08-26-kernel-library-split-implementation.md](../superpowers/plans/2026-08-26-kernel-library-split-implementation.md)（P1–P5 含 CMake 模板与验收命令） |

历史基线（Phase 0～4 已大部分落地，**§1～3 的「当前结构」勿当现状**）：[engineering-refactoring-plan.md](engineering-refactoring-plan.md)。

NURBS **建模/自由曲面 UI** 未立项；正式 ADR 编号为 **0011**（勿占用 0010）。占位：[nurbs-modeling-ui-roadmap.md](../superpowers/specs/2026-08-24-nurbs-modeling-ui-roadmap.md)。

---

## 1. 原则

1. **同仓多 CMake target，不拆 Git 仓库。** Guid/Log 必须全进程同一类型；独立仓库解决不了共享。
2. **一层一个变化原因。** 几何拓扑、显示网格、布尔求交、特征再生、Qt 壳，各自独立变。
3. **依赖单向、无环。** SHARED 下环依赖无法链接（ADR 0003）。
4. **三种真相分开，禁止互相写回：**
   - **特征历史**（`FeatureTree` / 再生）— 怎么来的  
   - **精确 B-Rep**（`Model` 池）— 现在是什么  
   - **显示 mesh**（`TriangleMesh` / `.bks`）— 怎么画的  
5. **Viewer 生产代码只 `#include "api/..."`。** 实现细节在 `kernel/internal/`（PRIVATE）。`bootstrap/` 可 include Hypodermic，仍不得 include `brep/internal`。
6. **长期身份用 `brep::Guid`。** ECS / 命令 / undo 存 Guid，用时 `FindBody`；禁止跨层缓存 `Body*`。
7. **日志是进程级基础设施。** Kernel 与 Viewer 共用 `BREP_*` / `InitLogging`；禁止再包 `ViewerGuid` 或平行 Qt 日志门面去转内核类型。
8. **内核用工厂+接口组合，Viewer 用 Hypodermic 组合。** `Part` 注入 `IBooleanEvaluator`；Container 只出现在 Composition Root（`viewer_bootstrap`）。
9. **先改链接图，后搬家目录。** 源码路径可暂时不动。
10. **第二个客户端（CLI）出现之前，不抽顶层 `adapter/`。** 无头程序直接链 `brep` / `brep_feat`。

---

## 2. 文档怎么读（避免过期图）

| 要解决的问题 | 读 |
|--------------|-----|
| 整仓该长什么样 | **本文** |
| 这个类型归哪个 `api/*.h`、哪条 DLL（**当前代码**） | [api-module-owners.md](api-module-owners.md) |
| IoC 能不能 resolve、Scope | [ioc-governance.md](ioc-governance.md) |
| 拓扑指针谁拥有 | [AGENTS.md](../../AGENTS.md)「拓扑与生命周期」 |
| 2016-08 工程化任务清单 / 大文件拆分史 | engineering-refactoring-plan（历史） |
| 布尔算法阶段 | ADR 0006/0008 + superpowers 布尔设计/计划 |
| NURBS 显示参数与 Viewer | ADR 0009 + nurbs-unified-strategy |
| Adapter 禁止按类型长接口；属性面板 PropertySheet | [viewer-adapter-governance.md](viewer-adapter-governance.md) §9 |

---

## 3. 现状 vs 目标

### 3.1 已经合理（不要重拆）

```text
brep_viewer.exe
  └─ viewer_ui
       ├─ viewer_bootstrap   STATIC，Hypodermic Composition Root（ADR 0007）
       └─ viewer_runtime     scene + commands + render 合一（ADR 0003）
            └─ viewer_adapter
                 └─ brep INTERFACE → io / asm / feat / core
```

`viewer_runtime` **维持单体 DLL**：scene / commands / render 源码互引，拆三个 SHARED 会环依赖。

### 3.2 缺口：`brep_core` 仍是厨房水槽

`cmake/BrepCore.cmake` 把 Guid/Log、几何拓扑、Mesh/CDT、FaceBvh、SnapQuery、**整份 bool/** 编进同一库。  
工程化方案 §3.2 的逻辑分层（math → geom → mesh → feat）**从未落到 CMake**。这是 ADR 0010 要修的唯一内核结构债。

### 3.3 目标 DAG

```text
                    brep_viewer.exe
                            │
                       viewer_ui
                      /          \
           viewer_bootstrap    viewer_runtime
           (IoC 装配)          (ECS / 命令 / Vulkan)
                      \          /
                     viewer_adapter
                            │  仅 api/*
        ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┼ ─ ─ ─ ─ ─ ─ ─ ─ ─
                            ▼
                     brep  INTERFACE（兼容旧链接名，聚合下列全部）

         brep_io                brep_asm              brep_feat
         (.xl / .bks)           (装配)                (Part / Document / 特征)
           │     │                  │                      │
           │     └──── feat/asm ────┴──────────┬───────────┘
           │                                   ▼
           │                              brep_bool
           ▼                              (Pipeline)
      brep_mesh
      (tessellate / CDT / .bks)
           │                                   │
           └────────────────┬──────────────────┘
                            ▼
                       brep_core
                       几何 + 拓扑 + Model
                            ▼
                       brep_base
                       Math / Guid / Log
```

`SnapQuery`、`FaceBvh` **第一期留在 `brep_core`**。没有第二种链接需求前，不抽 `brep_snap` / `brep_spatial`。

`sketch` / `solve2d` **留在 `brep_feat`**（服务再生，不是第三套几何内核）。

---

## 4. 各层职责

### 4.1 `brep_base` — 进程级公共件

| 有 | 没有 |
|----|------|
| `Math`、`Guid`、`Log` / `BREP_*`、`InitLogging` | `Body`、曲面求交、网格、Qt、Hypodermic |

Viewer / adapter / 插件 **合法依赖这一层的类型**，不是「越层使用内核私货」。  
**不要**做成可替换产品 `Guid.dll`。

### 4.2 `brep_core` — 精确 B-Rep

几何、拓扑、`Model`、`Builder`、`Validate`、`Dump`、`IObject`、`ObjectRegistry`、`Material`。  
所有权：`Model` 拥有实体池；图上 `Edge*` / `CoEdge::Next` 为观察指针（R.3）。  
**没有** Pipeline、CDT、特征树、`.xl`。

NURBS **曲线/曲面存储与求值** 落在这一层（与 Plane/Sphere 相同）；**三角化** 不在这一层。

### 4.3 `brep_mesh` — 显示与导出网格

`tessellate_body`、`extract_edges`、CDT、`LoopSample`、ADR 0009 的 NURBS 显示 tessellate / 等参线。  
依赖 `brep_core`，**禁止**依赖 `brep_bool`。  
**禁止**把三角网写回 B-Rep。屏幕质量、`.bks`、STL 导出参数可独立（见 NURBS 总纲）。

### 4.4 `brep_bool` — 精确拓扑上的集合运算

六阶段 Pipeline、`Intersect*`、Imprint、分类、选面、重建、`IBooleanEvaluator` **实现**。  
依赖 `brep_core`（含 `FaceBvh`），**禁止**依赖 `brep_feat` / `brep_mesh`。  
NURBS **求交/印记** 另开 ADR，实现仍进本库，不进 mesh。

内部头 `Fuzzy` / `Polygon2d`：`kernel/internal/`，PRIVATE；bool 与 mesh 都可 include，**不进** `api/`。

### 4.5 `brep_feat`

`Part`、`Document`、特征、参数、草图、求解。`Part` **构造注入** `IBooleanEvaluator`（默认 `MakeDefaultBooleanEvaluator()`，符号在 `brep_bool`）。  
**禁止**把 `Part` 搬回 core（ADR 0003 为消除 core↔feat 环才下放到 feat）。

### 4.6 `brep_asm` / `brep_io`

装配在 feat 之上；IO 链 feat + asm，并且 **PUBLIC 链 `brep_mesh`**（`BksCache` 调用 `TessellateBody` / `ExtractEdges`）。显示缓存 `.bks` 属于 io，不是 B-Rep 真相。

### 4.7 Viewer

| Target | 职责 | 约束 |
|--------|------|------|
| `viewer_adapter` | `IDocumentService` / `ISceneService`；Guid ↔ ECS；不长期持有 `Body*` | 只 `api/*`；链 `brep` 聚合即可；**禁止按几何类型长虚函数**（[治理](viewer-adapter-governance.md)、[执行计划](../superpowers/plans/2026-08-27-viewer-adapter-governance-implementation.md)） |
| `viewer_runtime` | ECS、命令、捕捉 UI、Vulkan | 经 adapter；可用 `brep::Guid` 与 `BREP_*` |
| `viewer_bootstrap` | 唯一 `ContainerBuilder::build()`；模块 10→40 注册 | **STATIC**；`runtime`/`adapter` 不链 Hypodermic |
| `viewer_ui` | Qt 外壳 | 不 include `brep/bool/**` 实现头；属性面板只渲染 `PropertySheet`（[治理 §9](viewer-adapter-governance.md)） |
| 插件 | `brep_register_module` | 只扩 Viewer 装配，不改内核 DAG |

第二个客户端出现前 **不**提炼 `apps/common`。

---

## 5. 组合根：工厂 vs IoC

```text
内核（无 Hypodermic）
  MakeDefaultBooleanEvaluator()     →  返回 IBooleanEvaluator
  Part(evaluator)                   →  构造注入，禁止 setter / 全局 factory 作为生产路径

Viewer Composition Root
  KernelServicesModule              →  只注册工厂能给出的内核服务
                                      （IBooleanEvaluator、可选 IntersectorRegistry 配置）
                                      禁止注册 ImprintEngine、Pipeline 内部类型、Face*、Body*
  ViewerAdapterModule               →  IDocumentService；ISceneService 为 per-Document factory
  ViewerRuntimeModule               →  CommandRegistry 等
  ViewerUiModule                    →  窗口依赖聚合
```

| Scope | 可以 | 不可以 |
|-------|------|--------|
| Application `.singleInstance()` | evaluator、命令注册表 | `Document` / `Part` / `Model` / `ISceneService` |
| Document | 外部 `shared_ptr`，factory(Document*) | 塞进全局容器当单例 |
| View | `VulkanRenderer`（绑 `QWindow`） | QObject 由 Container 当 parent |

拆出 `brep_bool` 之后，**不要**用 IoC 把 Pipeline 零件再粘成一张应用级服务图。细节：[ioc-governance.md](ioc-governance.md)。

---

## 6. 公开 API 与模块（目标）

过渡期 `api/Core.h` 仍直接带 Guid/Log，Viewer 零改 include。落地后：

| 聚合头 | 稳定级 | CMake | 内容 |
|--------|--------|-------|------|
| `api/Base.h`（新增） | 高 | `brep_base` | Guid、Log、Math |
| `api/Core.h` | 高 | `brep_core` | Base + 几何/拓扑/`Model`/`IObject` |
| `api/Mesh.h` | 中 | `brep_mesh` | 三角化 / 边提取 |
| `api/Modeling.h` | 中 | `brep_feat` | Document/Part/特征；**只** include evaluator 接口，不 include `brep/bool/Boolean.h` 伞头 |
| `api/Boolean.h`（可选，窄） | 中 | `brep_bool` | `IBooleanEvaluator`、`BooleanOp`、工厂 |
| `api/Persistence.h` | 低 | `brep_io` | xl / bks |

`Boolean.h` 伞头继续给 examples / kernel 测试用 Pipeline 内部类型。  
当前代码归属仍以 [api-module-owners.md](api-module-owners.md) 为准（Mesh/Guid 暂记在 `brep_core`）。

---

## 7. 能力落点（避免「都是 NURBS 就进同一个库」）

| 能力 | 层 |
|------|-----|
| `NurbsCurve` / `NurbsSurface` 存储与 `Evaluate` | `brep_core` |
| 显示 tessellate、等参线、控制网、质量预设消费 | `brep_mesh` + Viewer（只吃 `api/Mesh.h`） |
| 解析面布尔 | `brep_bool`（ADR 0008，进行中） |
| NURBS 求交 / 印记 | `brep_bool`（**另 ADR**，在显示与解析布尔稳定之后） |
| NURBS 特征 / CV 编辑 UI | `brep_feat` + `viewer_runtime`（**ADR 0011**，未排期） |

顺序：**解析布尔正确性 → CMake P1 `brep_bool` → NURBS 显示进 mesh → 再考虑 NURBS 建模/求交。**  
不并行铺「显示 + 求交 + 特征 UI」三轨。

---

## 8. 明确不做

- 多 Git 仓库、Superbuild、微服务、内核 Qt/Vulkan/EnTT/Hypodermic。
- 拆 `viewer_runtime` 为三个 SHARED DLL。
- 每个 `Intersect*.cpp` 或 Pipeline 阶段一个库；`brep_snap` / `brep_spatial`（无第二种客户端前）。
- 抽象跨 API 的 RHI。
- 顶层 `adapter/`、`brep_platform` SHARED（ADR 0007）。
- 把 Guid/Log 锁成「仅几何层能碰」。
- 为拆库改 `IBooleanEvaluator` 语义或拓扑所有权模型。

---

## 9. 现状代码对照（2026-08-26）

| 项 | 代码 |
|----|------|
| 内核 DLL（CMake） | `brep_base` `brep_core` `brep_mesh` `brep_bool` `brep_feat` `brep_asm` `brep_io` |
| Guid / Log | `brep_base`（`src/base/Guid.cpp`、`src/base/Log.cpp`） |
| `src/bool/*` | `brep_bool` |
| Mesh / CDT | `brep_mesh`（`src/mesh/Mesh.cpp`、`Cdt.cpp`、`LoopSample.cpp`） |
| `api/Modeling.h` | `api/Boolean.h`（Evaluator），不再 include `bool/Boolean.h` 伞头 |
| Viewer include | `api/*` + `include_boundaries` |
| IoC | `apps/viewer/bootstrap/` 已落地 |

验收：本机 `cmake --build cmake-build-mingw-debug` 后 `bin/` 出现 `libbrep_base.dll` / `libbrep_mesh.dll` / `libbrep_bool.dll`；`ctest` 全绿。核心判据：`brep_core` 不链接 `brep_bool`；改 `ImprintEngine.cpp` 不重编 `Geometry.cpp` / `Cdt.cpp`；ECS 仍用 `brep::Guid`。
