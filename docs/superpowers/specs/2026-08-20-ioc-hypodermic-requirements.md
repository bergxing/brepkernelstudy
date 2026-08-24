# IoC（Hypodermic）需求规格

**日期**：2026-08-20  
**状态**：草案（待 Phase 0 实施）  
**关联**：[ADR 0007](../../architecture/adr/0007-hypodermic-ioc-adoption.md)、[ioc-governance.md](../../architecture/ioc-governance.md)、[ADR 0006](../../architecture/adr/0006-boolean-pipeline-architecture.md)

---

## 1. 文档目的

定义 **brepkernelstudy 全项目引入 Hypodermic** 的功能需求、非功能需求、边界约束与验收标准，作为实施方案的唯一需求基线。

---

## 2. 背景与问题陈述

### 2.1 当前痛点

| ID | 问题 | 影响 |
|----|------|------|
| P1 | 布尔求值器装配分散（`Part` lazy init + 全局 factory） | 测试需 `FactoryGuard`；多后端难切换 |
| P2 | Viewer 命令在 `register_builtin_commands()` 硬编码 | 新命令需改组合根；无统一注入 |
| P3 | `SceneAdapter` / `DocumentService` 为具体类 | Adapter 测试无法 mock IO |
| P4 | ADR 0006 Pipeline/FastPath 已模块化，但无统一注册点 | 扩展 Stage/Intersector 仍靠手动 wiring |
| P5 | 未来 Box−Sphere、Split、可选 OCCT 需可插拔后端 | N×M 特解不可持续 |

### 2.2 目标用户

- **应用开发者**：在 Viewer 中新增 Command / Tool / 面板功能  
- **内核开发者**：注册 Boolean Pipeline、IO、Feature 相关服务  
- **测试维护者**：注入 Fake 实现，无需全局静态 override  

---

## 3. 目标（Goals）

| G1 | 统一 Composition Root，装配逻辑可追踪 |
| G2 | 核心服务面向接口编程（`IBooleanEvaluator` 等） |
| G3 | 测试可替换任意已注册服务 |
| G4 | Kernel 公开 API（`api/*`）不被 IoC 污染 |
| G5 | 与 ADR 0006 Boolean Pipeline 架构自然衔接 |
| G6 | 支持未来可选后端（如 OCCT `IBooleanEvaluator`）同接口注册 |

---

## 4. 非目标（Non-Goals）

| NG1 | Kernel 内部使用 Hypodermic |
| NG2 | 替换 EnTT ECS 或 FeatureTree 数据结构 |
| NG3 | 运行时动态加载 DLL 插件（Phase 5 可选，非首期） |
| NG4 | 微服务化 / 跨进程 DI |
| NG5 | 重写全部具体类为接口（分阶段接口化） |

---

## 5. 功能需求

### FR-1 Composition Root

| 编号 | 描述 | 优先级 |
|------|------|--------|
| FR-1.1 | 提供 `build_application_container(AppConfig)` 构建应用级 Container | P0 |
| FR-1.2 | 提供 `build_test_container(overrides)` 供测试 override | P0 |
| FR-1.3 | Container 在 Viewer 进程内 Singleton；Document 不 Singleton | P0 |
| FR-1.4 | 启动日志输出已注册模块列表（debug 级别） | P2 |

### FR-2 模块化注册（IModule）

| 编号 | 描述 | 优先级 |
|------|------|--------|
| FR-2.1 | 定义 `IModule::register_services(ContainerBuilder&)` | P0 |
| FR-2.2 | 模块按 `order()` 排序注册 | P0 |
| FR-2.3 | 至少四个模块：KernelServices、ViewerAdapter、ViewerRuntime、ViewerUi | P1 |
| FR-2.4 | 模块定义与 Kernel 源码分离（Kernel 仅提供 Factory 函数） | P0 |

### FR-3 Kernel 服务

| 编号 | 描述 | 优先级 |
|------|------|--------|
| FR-3.1 | 注册 `std::shared_ptr<IBooleanEvaluator>` 默认 `CompositeBooleanEvaluator` | P1 |
| FR-3.2 | `Part` **构造注入** `std::shared_ptr<IBooleanEvaluator>`；移除 lazy init 与 `set_boolean_evaluator()` | P1 |
| FR-3.3 | `Document::add_part` 创建 Part 时传入 evaluator（来自 Container resolve 或测试 Fake） | P1 |
| FR-3.4 | 保留 `make_default_boolean_evaluator()` 供 bootstrap Module 与无 Part 的 kernel 测试使用 | P1 |
| FR-3.5 | `set_boolean_evaluator_factory` 标记 deprecated，文档给出迁移路径 | P2 |

### FR-4 Viewer 服务

| 编号 | 描述 | 优先级 |
|------|------|--------|
| FR-4.1 | `ICommand` 实现由 ViewerRuntimeModule 注册 | P2 |
| FR-4.2 | `CommandManager` / `CommandRegistry` 由 Container resolve | P2 |
| FR-4.3 | `CommandContext` 通过工厂注入所需服务（Scene、Document、History） | P2 |
| FR-4.4 | `ISceneService` 抽象 `SceneAdapter` | P3 |
| FR-4.5 | `IDocumentService` 抽象 `DocumentService` | P3 |

### FR-5 测试与 Fake

| 编号 | 描述 | 优先级 |
|------|------|--------|
| FR-5.1 | `test_boolean_feature` 改用 TestContainer + Fake Evaluator | P1 |
| FR-5.2 | 测试 override 不得依赖全局静态 factory 顺序 | P1 |
| FR-5.3 | CI 现有 `ctest` 用例数不减少 | P0 |

---

## 6. 非功能需求

### NFR-1 性能

| 编号 | 描述 |
|------|------|
| NFR-1.1 | 热路径（布尔 evaluate、每帧 render）不得在循环内 `resolve()` |
| NFR-1.2 | Application 级服务启动时 resolve 一次并缓存 |
| NFR-1.3 | Container build 耗时 < 100ms（Debug，典型注册表规模） |

### NFR-2 可维护性

| 编号 | 描述 |
|------|------|
| NFR-2.1 | 每个注册点注释：类型、Scope、线程假设 |
| NFR-2.2 | 新增服务 checklist：接口 → 实现 → Module 注册 → 测试 override |
| NFR-2.3 | Hypodermic 版本 pin 在 **Git submodule** `third_party/hypodermic`（固定 commit） |

### NFR-3 兼容性

| 编号 | 描述 |
|------|------|
| NFR-3.1 | MinGW + Qt6 现有 preset 可编译 |
| NFR-3.2 | `BREP_BUILD_TESTS=OFF` 时可禁用 TestContainer 目标 |
| NFR-3.3 | 无 Container 的 kernel 单元测试仍可独立运行 |

### NFR-4 安全与边界

| 编号 | 描述 |
|------|------|
| NFR-4.1 | `check_include_boundaries.py` 扩展：禁止 viewer 非 bootstrap 引用 Hypodermic |
| NFR-4.2 | Kernel DLL 不增加 Hypodermic 符号依赖 |

---

## 7. 系统上下文

```text
                    ┌─────────────────────────────────┐
                    │  brep_viewer (Composition Root) │
                    │  Hypodermic::Container           │
                    └───────────────┬─────────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
  ViewerUiModule            ViewerRuntimeModule          ViewerAdapterModule
  MainWindow deps           CommandManager               ISceneService
                            ICommand[]                   IDocumentService
         │                          │                          │
         └──────────────────────────┼──────────────────────────┘
                                    ▼
                          KernelServicesModule
                          IBooleanEvaluator (→ brep_core 工厂)
                                    │
                                    ▼
                          libbrep_core / libbrep_feat (无 Hypodermic)
```

---

## 8. 接口清单（分阶段）

### Phase 1 必须

| 接口 | 实现 | 注册名 |
|------|------|--------|
| `IBooleanEvaluator` | `CompositeBooleanEvaluator` | Application Singleton |

### Phase 2～3

| 接口 | 实现 |
|------|------|
| `ICommand` | 各 builtin command |
| `ISceneService` | `SceneAdapter` |
| `IDocumentService` | `DocumentService` |

### 未来

| 接口 | 用途 |
|------|------|
| `ISplitEvaluator` | 曲面裁剪 |
| `IBooleanEvaluator` | OCCT 可选后端 |
| `IModule` | 动态插件导出 |

---

## 9. 约束与假设

- 假设 Hypodermic 持续兼容 C++20 / MinGW（风险见 ADR 0007 修订触发）  
- 假设单进程单 Viewer 实例为主场景（多文档为 Phase 4）  
- 约束：Viewer 仍通过 `api/*` 访问内核类型  
- 约束：ADR 0005 自研布尔路线不变；IoC 仅改变 **装配**，不改变算法  

---

## 10. 验收标准

### Phase 0 完成

- [ ] Git submodule `third_party/hypodermic` 已登记，`cmake/Hypodermic.cmake` 可链接；viewer/tests 编译成功  
- [ ] `build_application_container` 返回非空 Container，零业务行为变更  
- [ ] 文档 trio：ADR 0007、本需求、实施方案、ioc-governance 已入库  

### Phase 1 完成

- [ ] `Part` 构造函数要求 `shared_ptr<IBooleanEvaluator>`；无 setter 路径  
- [ ] `Document::add_part` 传入 evaluator  
- [ ] `test_boolean_feature` 无 `set_boolean_evaluator_factory`  
- [ ] 现有 boolean 相关 ctest 全绿  

### Phase 2 完成

- [ ] `register_builtin_commands` 逻辑迁入 Module  
- [ ] MainWindow 不直接 `new CommandManager`（或仅保留 thin wrapper）  

### Phase 3 完成

- [ ] Save/Open 命令使用 `IDocumentService`  
- [ ] `viewer_adapter_tests` 可注入 Fake DocumentService  

---

## 11. 风险登记

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| Hypodermic 编译慢 | 中 | 中 | 限制在 viewer/tests；kernel 不 include |
| 过度 DI | 中 | 高 | ioc-governance 禁止项；Code Review checklist |
| 迁移周期长 | 高 | 中 | 分 Phase；每 Phase 可 ship |
| 与 Qt 生命周期冲突 | 中 | 高 | QObject 不由 Container 管理 parent |
| 循环依赖 | 低 | 高 | 接口拆分 + registerFactory |

---

## 12. 已确认决策（2026-08-20）

| # | 决策 |
|---|------|
| D1 | Hypodermic 以 **Git submodule** 引入，路径 `third_party/hypodermic` |
| D2 | `Part` 采用 **构造注入** `IBooleanEvaluator`，不采用 setter 过渡 |

---

## 13. 术语

| 术语 | 含义 |
|------|------|
| IoC | 控制反转：创建与装配由外部容器负责 |
| DI | 依赖注入：通过构造/工厂传入依赖 |
| Composition Root | 组合根：唯一调用 `ContainerBuilder::build()` 的位置 |
| Scope | 服务生命周期（Singleton / Transient / Document） |
| Module | `IModule`：一组相关服务的注册单元 |
