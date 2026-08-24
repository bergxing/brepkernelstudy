# IoC（Hypodermic）实施方案

> **需求基线：** `docs/superpowers/specs/2026-08-20-ioc-hypodermic-requirements.md`  
> **治理规范：** `docs/architecture/ioc-governance.md`  
> **决策记录：** `docs/architecture/adr/0007-hypodermic-ioc-adoption.md`

**Goal：** 在 **不污染 Kernel 公开 API** 的前提下，用 Hypodermic 统一 Viewer 与子系统测试的依赖装配，并逐步替换全局 factory / 静态命令注册。

**Architecture：** Kernel 暴露接口 + 工厂；`apps/viewer/bootstrap/` 为唯一 Composition Root；按 `IModule` 分模块注册；测试用 `TestContainer` override。

**Tech Stack：** C++20、CMake、Hypodermic（**Git submodule** `third_party/hypodermic`）、Qt6 Viewer、GoogleTest、现有 MinGW preset `cmake-build-mingw-debug`。

---

## Global Constraints

- Kernel `brep_core` / `brep_feat` / `brep_io` **不链接** Hypodermic。
- Viewer 生产代码（除 `bootstrap/`）**不 include** `<Hypodermic/...>`。
- 热路径禁止循环内 `resolve()`。
- 每 Phase 合并后 **现有 ctest 不回归**。
- 不提交 build 树产物；Hypodermic 版本写入 `cmake/Hypodermic.cmake`。
- 用户可见字符串仍走 i18n；IoC 不涉及 UI 文案变更。

---

## 目标架构（落地后）

```text
apps/viewer/main.cpp
  └─ ViewerApp
       └─ build_application_container(config)
            ├─ KernelServicesModule
            ├─ ViewerAdapterModule
            ├─ ViewerRuntimeModule
            └─ ViewerUiModule
       └─ MainWindow(container, ...)
            └─ resolve CommandManager, ...
```

---

## File Map（计划新增/修改）

| 文件 | 职责 |
|------|------|
| `cmake/Hypodermic.cmake` | 子模块路径 + `Hypodermic::Hypodermic` INTERFACE target |
| `.gitmodules` | 登记 `third_party/hypodermic` |
| `third_party/hypodermic` | Hypodermic 源码（git submodule） |
| `cmake/BrepPlatform.cmake` | 可选 `brep_platform` INTERFACE（聚合 IoC 无关工具） |
| `apps/viewer/bootstrap/i_module.hpp` | `IModule` 接口 |
| `apps/viewer/bootstrap/application_container.hpp/.cpp` | Composition Root |
| `apps/viewer/bootstrap/test_container.hpp/.cpp` | 测试 Container |
| `apps/viewer/bootstrap/kernel_services_module.hpp/.cpp` | 注册 `IBooleanEvaluator` 等 |
| `apps/viewer/bootstrap/viewer_adapter_module.hpp/.cpp` | Phase 3：`ISceneService` |
| `apps/viewer/bootstrap/viewer_runtime_module.hpp/.cpp` | Phase 2：Commands |
| `apps/viewer/bootstrap/viewer_ui_module.hpp/.cpp` | Phase 2：UI 依赖 |
| `apps/viewer/adapter/iscene_service.hpp` | Phase 3 接口 |
| `apps/viewer/adapter/iDocumentService.h` | Phase 3 接口 |
| `apps/viewer/app/ViewerApp.cpp` | 调用 `build_application_container` |
| `apps/viewer/MainWindow.cpp` | 接收 Container 或 ServiceLocator 封装 |
| `kernel/include/brep/Part.h` | Phase 1：构造注入 `shared_ptr<IBooleanEvaluator>` |
| `kernel/src/part.cpp` | 移除 lazy factory、`set_boolean_evaluator` |
| `kernel/include/brep/Document.h` | `add_part` 传入 evaluator |
| `scripts/check_include_boundaries.py` | 扩展 Hypodermic 边界规则 |
| `tests/kernel/test_ioc_test_container.cpp` | Phase 0 smoke（可选放 viewer tests） |

---

## Phase 0 — 基础设施（零行为变更）

**退出标准：** Viewer 可启动；ctest 与迁移前一致；Container 可 build/resolve 空服务或 stub。

### Task 0.1：Git 子模块 + CMake

**Files：** `.gitmodules`、`third_party/hypodermic`、`cmake/Hypodermic.cmake`、根或 `apps/viewer/CMakeLists.txt`

- [ ] 添加 submodule：`git submodule add https://github.com/ybainier/Hypodermic.git third_party/hypodermic`
- [ ] Pin 到稳定 release commit（在实施方案 PR 说明中记录 hash）
- [ ] `cmake/Hypodermic.cmake`：`add_subdirectory` 或 INTERFACE include + alias `Hypodermic::Hypodermic`
- [ ] `viewer_runtime` / `viewer_ui` / `brep_viewer` 链接 Hypodermic
- [ ] `brep_core` 等 kernel target **不** link
- [ ] README / 克隆说明：`git submodule update --init --recursive`

**`cmake/Hypodermic.cmake` 参考形态：**

```cmake
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/hypodermic/CMakeLists.txt")
  message(FATAL_ERROR "Hypodermic submodule missing. Run: git submodule update --init third_party/hypodermic")
endif()
add_subdirectory(third_party/hypodermic EXCLUDE_FROM_ALL)
# 或 header-only：INTERFACE target 指向 include 目录
```

### Task 0.2：Bootstrap 骨架

**Files：** `apps/viewer/bootstrap/*`

- [ ] `IModule`：`register_services(ContainerBuilder&)` + `order()`
- [ ] `build_application_container()`：按 order 注册模块，返回 `shared_ptr<Container>`
- [ ] `build_test_container(overrides)`：空模块 + 可选 lambda override
- [ ] `KernelServicesModule` 空实现（或仅注册 noop 诊断服务）

### Task 0.3：ViewerApp 接线（不改变 MainWindow 行为）

**Files：** `ViewerApp.cpp`

- [ ] 启动时 build Container，存入 `ViewerApp` 或 `ApplicationContext`
- [ ] MainWindow 暂不 resolve；Container 仅持有备用

### Task 0.4：文档与 CI

- [ ] ADR 0007、需求、治理、本方案已入库（本任务）
- [ ] `ctest` 全量通过
- [ ] README 或 `docs/architecture/` 增加 IoC 文档索引一行

**验证：**

```powershell
cmake --build cmake-build-mingw-debug --target brep_viewer -j 8
cmake-build-mingw-debug/bin/brep_viewer.exe   # 手动 smoke：能启动
ctest -C Debug --output-on-failure
```

---

## Phase 1 — Kernel 布尔服务注入

**退出标准：** `Part` 不再 lazy 调用 `make_default_boolean_evaluator()`；`test_boolean_feature` 用 TestContainer。

### Task 1.1：KernelServicesModule 注册 Evaluator

**Files：** `kernel_services_module.cpp`、`CompositeEvaluator.hpp`

- [ ] 注册 Factory：`[] { return make_composite_boolean_evaluator(); }` → `shared_ptr<IBooleanEvaluator>`，singleInstance
- [ ] 文档注释 Scope + 线程安全

### Task 1.2：Part 构造注入

**Files：** `Part.h`、`part.cpp`、`Document.h`、`document.cpp`

**已确认：构造注入（无 setter 过渡）**

```cpp
// Part.h — 目标 API
explicit Part(std::shared_ptr<boolean::IBooleanEvaluator> evaluator,
                std::string part_name = "Part");
```

- [ ] `Part` 删除默认无 evaluator 的隐式构造；或保留 private + `Document::add_part` 为唯一工厂
- [ ] 成员 `std::shared_ptr<boolean::IBooleanEvaluator> evaluator_` 必填
- [ ] `boolean_evaluator()` 直接返回 `*evaluator_`（非空断言）
- [ ] 移除 `set_boolean_evaluator()`、`Part` 内 lazy `make_default_*`
- [ ] `Document::add_part(name, evaluator)` 或 `Document` 持有 `shared_ptr<IBooleanEvaluator>` 并在 `add_part` 时传给 Part
- [ ] `ViewerApp` / `World::create_blank_scene`：从 Container resolve evaluator 后创建 Document/Part
- [ ] 所有 `Part` 直构造 call site 更新（tests、examples、`Document::create` 路径）

**测试构造 Part：**

```cpp
auto eval = boolean::make_default_boolean_evaluator();
Part part(eval, "MainPart");
// 或 TestContainer resolve 后传入
```

### Task 1.3：测试迁移

**Files：** `TestBooleanFeature.cpp`、`test_container.hpp`

- [ ] 删除 `FactoryGuard` + `set_boolean_evaluator_factory`
- [ ] 测试用 `make_default_boolean_evaluator()` 或 `build_test_container` resolve 后传入 `Part` 构造函数
- [ ] 删除对 `part.set_boolean_evaluator()` 的依赖

### Task 1.4：Deprecate 全局 factory

**Files：** `EvaluatorStub.cpp`、`Evaluator.h`

- [ ] `set_boolean_evaluator_factory` 加 `[[deprecated]]` 注释与迁移说明
- [ ] 日志 warn：首次调用 deprecated API

**验证：**

```powershell
cmake-build-mingw-debug/bin/brep_test_boolean_feature.exe
cmake-build-mingw-debug/bin/brep_test_boolean_pipeline.exe
# Viewer：布尔运算 smoke（球先选、盒后选 Subtract）
```

---

## Phase 2 — Viewer 命令层

**退出标准：** `register_builtin_commands()` 迁到 `ViewerRuntimeModule`；MainWindow 从 Container 取 `CommandManager`。

### Task 2.1：Command 注册表模块

**Files：** `viewer_runtime_module.cpp`、`CommandRegistry.cpp`

- [ ] 每个 builtin command 注册为 transient factory 或统一 `CommandFactory`
- [ ] `CommandRegistry` singleInstance
- [ ] `CommandManager` 构造注入 `CommandRegistry&`

### Task 2.2：CommandContext 工厂

**Files：** `CommandTypes.h`、`CommandsBridge.cpp`

- [ ] `CommandContextFactory` 持有 `World*`、`DocumentSession*`、`ISceneService*` 等
- [ ] `execute` 时填充 Context，避免 Command 内 resolve

### Task 2.3：MainWindow 瘦身

**Files：** `MainWindow.cpp`

- [ ] 删除或薄化 `register_builtin_commands()` 直接调用
- [ ] 构造函数参数：`shared_ptr<Container>` 或预解析的 `CommandManager&`

**验证：**

```powershell
cmake-build-mingw-debug/bin/viewer_adapter_tests.exe
# 手工：Fuse / Cut / Common 菜单可用
```

---

## Phase 3 — Adapter 接口化

**退出标准：** Save/Open 使用 `IDocumentService`；Scene 操作使用 `ISceneService`。

### Task 3.1：定义接口

**Files：** `iscene_service.hpp`、`iDocumentService.h`

- [ ] 从现有 `SceneAdapter` / `DocumentService` 公共方法提取纯虚接口
- [ ] `SceneAdapter` / `DocumentService` 实现接口

### Task 3.2：ViewerAdapterModule

- [ ] 注册 `ISceneService` → `SceneAdapter`（注意 Document 生命周期：transient 或 factory  per document）
- [ ] 注册 `IDocumentService` → `DocumentService` singleInstance

### Task 3.3：命令与测试

**Files：** `BuiltinCommands.cpp`、`TestDocumentService.cpp`

- [ ] Save/Open resolve `IDocumentService`
- [ ] Adapter tests 可注入 Fake

---

## Phase 4 — Document 作用域（可选）

**退出标准：** 多文档场景下 Part/Document 不泄漏为全局 Singleton。

### Task 4.1：子 Container 或 InstanceScope

- [ ] 每个 `Document` 创建 child builder，注册 Document-scoped 服务
- [ ] `ISceneService` 与 `Document` 1:1

### Task 4.2：ViewManager 集成

- [ ] 每 View 窗口 resolve 对应 Document 子 Container

---

## Phase 5 — 插件与可选后端（可选）

### Task 5.1：第二 Evaluator 注册

- [ ] `IBooleanEvaluator` 多实现 + 配置选择（为 OCCT 预留，不实现 OCCT）

### Task 5.2：动态模块

- [ ] `extern "C" void brep_register_module(IModule&)`
- [ ] Composition Root 加载插件 DLL 列表

---

## 迁移对照表

| 现有代码 | Phase | 迁移后 |
|----------|-------|--------|
| `Part()` 默认构造 + lazy eval | 1 | `Part(shared_ptr<IBooleanEvaluator>, name)` |
| `Part::set_boolean_evaluator` | 1 | 删除 |
| `set_boolean_evaluator_factory` | 1 | deprecated → TestContainer |
| `register_builtin_commands()` | 2 | ViewerRuntimeModule |
| `SceneAdapter scene(doc)` 栈对象 | 3 | `ISceneService` |
| `DocumentService{}` 临时栈 | 3 | `IDocumentService` |
| `make_default_boolean_evaluator()` | 保留 | Module 内部调用 |

---

## 测试计划

| 阶段 | 新增/修改测试 |
|------|--------------|
| 0 | 可选：`bootstrap/test_container_smoke` |
| 1 | 改 `test_boolean_feature`；跑全量 boolean ctest |
| 2 | `viewer_adapter_tests` 命令相关 |
| 3 | Fake `IDocumentService` roundtrip |
| 4 | 多 Document 单元测试 |

**回归命令：**

```powershell
ctest -C Debug -R "boolean|SceneAdapter|include_boundaries" --output-on-failure
```

---

## 工时估算（人天，仅供参考）

| Phase | 估算 | 依赖 |
|-------|------|------|
| 0 | 2～3 | 无 |
| 1 | 3～5 | 0 |
| 2 | 4～6 | 1 |
| 3 | 4～5 | 2 |
| 4 | 5～8 | 3 |
| 5 | 8+ | 1 |

建议 **Phase 0 + 1** 作为第一个 PR；Phase 2/3 可并行准备接口设计。

---

## 已确认决策

| # | 决策 | 日期 |
|---|------|------|
| D1 | Hypodermic：**Git submodule** → `third_party/hypodermic` | 2026-08-20 |
| D2 | **Part 构造注入** `shared_ptr<IBooleanEvaluator>`，无 setter 过渡 | 2026-08-20 |

## 待确认（可选）

| # | 问题 | 建议默认 |
|---|------|----------|
| Q1 | `SceneAdapter` 是否 per-Document？ | 是；Phase 3 factory 传入 `Document*` |
| Q2 | Container 存 `ViewerApp` 还是 `ApplicationContext`？ | 独立 `ApplicationContext` struct |
| Q3 | 是否新增 `brep_platform` static lib？ | Phase 0 否；bootstrap 仅在 viewer |

---

## 文档索引

| 文档 | 路径 |
|------|------|
| ADR | `docs/architecture/adr/0007-hypodermic-ioc-adoption.md` |
| 需求 | `docs/superpowers/specs/2026-08-20-ioc-hypodermic-requirements.md` |
| 治理 | `docs/architecture/ioc-governance.md` |
| 本方案 | `docs/superpowers/plans/2026-08-20-ioc-hypodermic-implementation.md` |
| Boolean Pipeline | `docs/architecture/adr/0006-boolean-pipeline-architecture.md` |
