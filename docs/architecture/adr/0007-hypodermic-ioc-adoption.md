# ADR 0007: 引入 Hypodermic 作为 Viewer 层 IoC 容器

**状态**：已接受（文档阶段；代码未落地）  
**日期**：2026-08-20  
**前置**：[ADR 0006](0006-boolean-pipeline-architecture.md)、[engineering-refactoring-plan.md](../engineering-refactoring-plan.md)、[api-module-owners.md](../api-module-owners.md)

## 背景

项目 CMake 分层已稳定（`brep_core` → `brep_feat` → `brep_io` → `brep`；Viewer `viewer_adapter` → `viewer_runtime` → `viewer_ui`）。但 **依赖装配仍分散**：

- `Part::boolean_evaluator()` 懒加载 + 全局 `set_boolean_evaluator_factory()`
- `MainWindow` / `register_builtin_commands()` 静态注册命令
- `SceneAdapter` / `DocumentService` 为具体类，测试难以替换

ADR 0006 已引入 `CompositeBooleanEvaluator` + `BooleanPipeline` + `AnalyticFastPathRegistry`，适合作为 **可注册服务** 的首批对象。随着 Box↔Sphere 通用布尔、Split、可选后端等扩展，需要统一的 **Composition Root** 与 **测试注入** 机制。

## 决策

1. **采用 [Hypodermic](https://github.com/ybainier/Hypodermic)** 作为 Viewer 与子系统测试的 IoC 容器（header-only，C++17+，与项目 C++20 兼容）。
2. **Composition Root 唯一**：仅 `apps/viewer/bootstrap/`（或等价目录）允许 `ContainerBuilder::build()`；业务代码禁止自行 `new` 全局服务。
3. **Kernel 不链接 Hypodermic**：`kernel/include/brep/**` 与 `api/**` 不得 `#include <Hypodermic/...>`；内核继续暴露 **工厂函数 + 接口**（如 `IBooleanEvaluator`）。
4. **模块化注册**：按 CMake target 划分 `IModule`（`KernelServicesModule`、`ViewerRuntimeModule` 等），在 Composition Root 按序调用。
5. **生命周期治理**：Document/Part/Model **禁止** Application Singleton；详见 [ioc-governance.md](../ioc-governance.md)。
6. **Hypodermic 以 Git 子模块引入**：路径 `third_party/hypodermic`，与现有 `third_party/eigen`、`entt` 等策略一致；**不使用** FetchContent 拉取。
7. **`Part` 构造注入**：`IBooleanEvaluator`（及后续内核服务）通过 `Part` 构造函数传入；**不采用** setter 过渡方案。
8. **渐进迁移**：分 Phase 0～5 实施，每阶段可独立合并；不要求 Big Bang 重写。

## 理由

- **Hypodermic** 提供运行时注册、工厂、单例/瞬态生命周期，适合 Viewer 命令插件化与未来可选后端切换。
- 与 **Boost.ext.DI / Fruit** 相比，Hypodermic 更贴近「容器 + 模块注册」模型，学习曲线低于编译期 DI，且 **不要求 kernel 模板膨胀**。
- 与 **轻量自建 Registry** 相比，Hypodermic 可处理复杂依赖图，减少手写 `register_*` 顺序错误。
- Kernel 保持纯净，符合 ADR 0002 分级 API 与 `check_include_boundaries.py` 约束。
- **Git 子模块** pin 固定 commit/tag，离线/CI 可复现，与 monorepo 现有 `third_party/*` 治理一致。
- **构造注入**使 `Part` 依赖在创建时即明确，避免 lazy init 与半初始化状态。

## 后果

- **做**：新增 `docs/superpowers/specs/2026-08-20-ioc-hypodermic-requirements.md`（需求）、`docs/superpowers/plans/2026-08-20-ioc-hypodermic-implementation.md`（实施方案）、`docs/architecture/ioc-governance.md`（治理规范）。
- **后续做**：`cmake/Hypodermic.cmake`（子模块路径）、`git submodule` 登记、`apps/viewer/bootstrap/`、接口化 `ISceneService` / `IDocumentService`。
- **不做（本 ADR）**：Kernel 内嵌 Hypodermic；全局 Service Locator；Document 级子容器（Phase 4 可选）。

## 修订触发

- Hypodermic 维护停滞或 C++20 兼容问题 → 评估回退轻量 Registry 或 Fruit。
- 若 CLI / 无头批处理成为一等公民 → 增加独立 Composition Root（共享 `platform/` 模块定义）。
