# IoC 治理规范（Hypodermic）

**版本**：1.2  
**日期**：2026-08-26  
**关联**：[ADR 0007](adr/0007-hypodermic-ioc-adoption.md)、[layering.md](layering.md) §5

本文是开发时的 **快速约束清单**；需求细节见 [IoC 需求规格](../superpowers/specs/2026-08-20-ioc-hypodermic-requirements.md)。分层总图以 [layering.md](layering.md) 为准。

---

## 1. 分层与 include 边界

| 区域 | 可 include Hypodermic | 可 resolve 服务 |
|------|----------------------|-----------------|
| `kernel/include/brep/**` | **否** | **否** |
| `kernel/include/api/**` | **否** | **否** |
| `kernel/src/**` | **否** | **否** |
| `apps/viewer/bootstrap/**` | **是** | **是**（Composition Root） |
| `apps/viewer/app/**`（`ApplicationContext`） | 否 | 否（仅持有 Container 指针） |
| `apps/viewer/commands/**` | 否（仅通过 Context 注入） | 否 |
| `apps/viewer/ui/**` | 否 | 否 |
| `tests/**` | **是**（TestContainer 工厂） | **是** |

Viewer 生产代码继续遵守 `scripts/check_include_boundaries.py`：**只 include `api/*`**（bootstrap 层除外）。

---

## 2. Composition Root 与 ApplicationContext

- **唯一 build 入口**：`build_application_container(AppConfig)` → `std::shared_ptr<Hypodermic::Container>`
- **进程级持有**：`ApplicationContext` struct（`container` + `AppConfig`）；在 `run_viewer()` 早期构造，`MainWindow` / `HomeWindow` 接收 `ApplicationContext&`
- **禁止**：将 Container 存入 `ViewerApp` 函数静态、全局 Service Locator，或 `MainWindow` 成员以外散落 resolve
- **调用点**：`run_viewer()` 内、`MainWindow` 构造之前完成 `build_application_container`
- **禁止**：在 `ICommand::execute`、`IFeature::rebuild`、`Part` 内调用 `container->resolve`

---

## 3. 生命周期（Scope）

| 级别 | 配置 | 示例 |
|------|------|------|
| Application | `.singleInstance()` | `IBooleanEvaluator`、`IntersectorRegistry`、`CommandRegistry`、`IDocumentService` |
| Transient | `.registerFactory` / 默认 transient | 各 `ICommand` 实现、`SaveDocumentCommand` |
| Document | 外部 `shared_ptr` 持有，**不进**全局 Singleton | `Document`、`Part`、`Model`、`ISceneService`（**per-Document factory**） |
| View | `MainWindow` / `ViewManager` 构造注入 | `VulkanRenderer`（绑定 `QWindow`） |

**`ISceneService`（Phase 3）**：与 `Document` **1:1**；在 `ViewerAdapterModule` 注册 **factory**（传入 `Document*`），**不得** `.singleInstance()`。每个 workspace / `World` 创建 Document 时取得对应 `ISceneService`。

门面职责与「禁止 `XxxSpecFor`」见 [viewer-adapter-governance.md](viewer-adapter-governance.md)。

---

## 4. 模块注册顺序

```text
10  KernelServicesModule     // IBooleanEvaluator 工厂、Pipeline 默认配置
20  ViewerAdapterModule      // ISceneService, IDocumentService（接口化后）
30  ViewerRuntimeModule      // CommandRegistry, CommandManager, ECS 服务
40  ViewerUiModule           // MainWindow 依赖聚合
```

后序模块可依赖前序模块注册的类型；**禁止**循环注册（用接口 + Factory 延迟解析）。

`KernelServicesModule` 只注册 **工厂能给出的内核服务**（如 `IBooleanEvaluator`）。**禁止**把 `ImprintEngine`、Pipeline 阶段对象、`Face*` / `Body*` 注册进容器。按 [ADR 0010](adr/0010-kernel-library-split.md) 拆开 `brep_bool` 之后，IoC **不得**再把内部类型粘成 Application 单例网。见 [layering.md](layering.md) §5。

---

## 5. 编码禁止项

1. 禁止 `kernel` 链接 Hypodermic target  
2. 禁止 Service Locator（全局 `get_container()`），测试除外且仅限 `test_container.hpp`  
3. 禁止在拓扑实体（`Body`/`Face`/…）上挂 Container 解析  
4. 禁止 `QObject` 子类由 Container 作为 parent 管理生命周期  
5. 禁止未定义 Scope 的注册（每个 `register*` 必须注释 Scope）

---

## 6. 测试

- 使用 `build_test_container(overrides)` 替代 `set_boolean_evaluator_factory`
- Kernel 纯几何测试可无 Container
- 集成测试通过 override 注册 Fake 实现

---

## 7. 与现有工厂的关系

| 现有机制 | 迁移目标 |
|----------|----------|
| `make_default_boolean_evaluator()` | KernelServicesModule 内 Factory 调用 |
| `set_boolean_evaluator_factory()` | **Deprecated** → TestContainer override |
| `Part::set_boolean_evaluator()` | **移除**（Phase 1）→ 仅构造注入 |
| `register_builtin_commands()` | ViewerRuntimeModule 注册 |

---

## 8. CMake 与 bootstrap 布局

- Phase 0 **不**新增 `brep_platform` SHARED/STATIC 产品库
- `apps/viewer/bootstrap/` → CMake target **`viewer_bootstrap`（STATIC）**，链接 `Hypodermic::Hypodermic`
- **`viewer_ui`** 链接 `viewer_bootstrap`；**`viewer_runtime` / `viewer_adapter` 不链接** Hypodermic
- bootstrap 为 **链接期合并**，非 ADR 0003 意义上的可替换 Viewer DLL 层
- 若未来 CLI 需共享装配：多个 exe **静态链接** 同一 `viewer_bootstrap`，仍不升为 SHARED DLL

---

## 9. 第三方依赖

- **Hypodermic** 位于 `third_party/hypodermic`（Git submodule）
- 克隆后执行：`git submodule update --init third_party/hypodermic`
- CMake 通过 `cmake/Hypodermic.cmake` 暴露 `Hypodermic::Hypodermic`；pin 版本记录在 submodule 所指向的 commit
