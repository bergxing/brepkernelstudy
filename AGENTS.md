# Agent 指南 — brepkernelstudy

## 项目结构

- `kernel/` — B-Rep 内核（`brep_base`, `brep_core`, `brep_mesh`, `brep_bool`, `brep_feat`, `brep_io`, `brep_asm`；见 [layering.md](docs/architecture/layering.md)）
- `apps/viewer/` — Qt6 + Vulkan Viewer
- `tests/` — GoogleTest 内核测试
- `docs/architecture/` — ADR 与工程规范

## C++ 规范

两套规范并行：**Google Style**（格式/命名/头文件）+ **Core Guidelines**（语义/所有权/接口）。

### Google C++ Style

- **Cursor rules**：`.cursor/rules/google-cpp-*.mdc`
- **Agent skill**：`.cursor/skills/google-cpp-style/SKILL.md`
  - 编写/审查 C++ 格式、命名、头文件、类布局时 **先读该 skill**
  - 项目约定见 `google-cpp-style/project-overrides.md`（**PascalCase 文件名 + `.h`/`.cpp`**、PascalCase 类/函数、camelCase 变量/参数、**类成员 `m_` + camelCase**、struct/DTO PascalCase、**4 空格缩进（禁止 2 空格/Tab）+ Allman**）
  - 迁移脚本：`scripts/migrate_style.py`（`.hpp`→`.h`、snake_case→PascalCase）、`scripts/fix_includes.py`（统一 `#include`）；**勿用**已废弃的 `fix_includes_to_hpp.py`、`format_allman_braces.py`（会破坏 `const`/`override`/`/*name*/`）。若已误跑，执行 `python scripts/repair_allman_corruption.py` 修复

### C++ Core Guidelines

- **Cursor rules**：`.cursor/rules/cpp-*.mdc`
- **Agent skill**：`.cursor/skills/cpp-core-guidelines/SKILL.md`
  - RAII、接口设计、错误处理时 **先读该 skill**
  - 项目特例见 `cpp-core-guidelines/project-overrides.md`

## 关键边界

- Viewer 生产代码仅 `#include "api/..."`（见 ADR 0002）
- 内核库分层（现行总图 [`docs/architecture/layering.md`](docs/architecture/layering.md)；CMake 拆分 ADR 0010；步骤 [`docs/superpowers/plans/2026-08-26-kernel-library-split-implementation.md`](docs/superpowers/plans/2026-08-26-kernel-library-split-implementation.md)）
- 布尔架构：ADR 0006（Pipeline 六阶段）、**ADR 0008**（仅通用路径，移除体对体特解）；设计/计划见 `docs/superpowers/specs/2026-08-24-boolean-general-pipeline-only-design.md`、`docs/superpowers/plans/2026-08-24-boolean-general-pipeline-implementation.md`；IoC：ADR 0007（Hypodermic，Viewer only）
- NURBS **显示**（与布尔解耦）：ADR 0009；建模/UI 远期占位用 **ADR 0011**（未立项）；总纲 `docs/superpowers/specs/2026-08-24-nurbs-unified-strategy.md`
- 构建：`cmake-build-mingw-debug`，MinGW 在 PATH

## 拓扑与生命周期

`Model` 用 `vector<unique_ptr<...>>` **拥有**几何/拓扑；`Topology.h` 里 `Edge*`、`CoEdge::Next` 等交叉引用是 **非拥有裸指针**（Core Guidelines R.3），**不要**改成 `shared_ptr`/`unique_ptr` 挂在图节点上。

### 所有权

| 层级 | 拥有 | 非拥有（观察） |
|------|------|----------------|
| `Model` | `m_vertices` / `m_edges` / `m_coedges` / … | — |
| `Vertex` / `Edge` / `CoEdge` / … | — | `V0`、`Next`、`Surface` 等图字段 |
| `Document` | `Part` | `ObjectRegistry` 中的 `IObject*` |
| Viewer ECS | — | `BodyRef{Guid}`（**不**长期缓存 `Body*`） |

### 指针有效范围

拓扑裸指针在 **`Model` 存活期间**有效；`RemoveBody()` 之后对应 **`Body*` 立即失效**（底层 Face/Edge 可能仍留在 Model 池里作 orphan，但不得再解引用旧 `Body*`）。

### 编码约定

1. **长期身份用 `Guid`**（或 `FeatureId`），用时 `Part::FindBody(guid)` / `ObjectRegistry::Find` 再解析；`FindBody` 返回 `nullptr` 表示已删或未再生。
2. **`Body*` / `Face*` / `Edge*`** 仅作 **单次算法/命令内的局部遍历**；禁止写入比 `Part`/`Document` 命更长的成员、undo 栈、全局缓存。
3. **删除/再生/布尔之后** 必须重新 `FindBody`，不得复用旧的 operand `Body*`。
4. **Viewer 选择、属性、ECS** 只存 `BodyRef.guid`（见 `apps/viewer/ecs/Components.h`），渲染/命令执行前通过 Guid 解析 Body。
5. **禁止** 从 Model 池单独 `delete` 某个 `Vertex`/`Edge` 而不更新整张半边图；删体走 `RemoveBody` / 特征 `RemoveFeature`，未来子图回收须做可达性分析。
6. 异步或跨帧任务 **不得捕获裸 `Body*`**；捕获 `Guid` + Document/Part 弱引用，执行前校验仍有效。

### 常见风险

| 场景 | 风险 | 规避 |
|------|------|------|
| Model 内局部遍历 | 低 | 保持指针在栈上、作用域内 |
| 缓存 `Body*` 后 `RemoveBody` | 野指针 | 改存 `Guid`，用时 `FindBody` |
| Model/Part 销毁后仍用拓扑指针 | 野指针 | 保证销毁顺序；不越级持有 |
| `RemoveBody` 后 orphan 几何 | 内存膨胀（非野指针） | `RemoveBody` 自动 `PurgeUnreferencedTopologyAndGeometry`；可用 `PoolStats()` 观测 |

## 测试

依赖 **GoogleTest**（`third_party/googletest`，git submodule，tag v1.15.2）。Configure 时**不再**从 GitHub 在线下载。

```powershell
git submodule update --init third_party/hypodermic
```

Hypodermic IoC (Viewer only, ADR 0007): `third_party/hypodermic` @ `ba5516d`. Hypodermic 还需要 **Boost 头文件**（signals2 / range / algorithm，header-only）。任选其一：

```powershell
# vcpkg（CMake 会自动探测 installed/x64-mingw-dynamic 或 x64-windows/include）
third_party/vcpkg/vcpkg install boost-signals2 boost-range boost-algorithm:x64-windows

# 或解压到 third_party/boost/
curl -L -o boost.tar.xz https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
tar -xf boost.tar.xz
move boost-1.84.0 third_party/boost
```

```powershell
git submodule update --init third_party/googletest
```

若 GitHub 不可达，submodule 已配置 Gitee 镜像。也可手动将 v1.15.2 解压到 `third_party/googletest/`。

临时跳过测试：`cmake -DBREP_BUILD_TESTS=OFF ...`

```powershell
ctest -C Debug --output-on-failure
ctest -R include_boundaries
```
