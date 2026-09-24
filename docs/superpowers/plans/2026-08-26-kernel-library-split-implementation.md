# 内核库拆分 — 详细实施方案

> **总图：** [`docs/architecture/layering.md`](../../architecture/layering.md)  
> **决策：** [`docs/architecture/adr/0010-kernel-library-split.md`](../../architecture/adr/0010-kernel-library-split.md)  
> **类型表（当前代码）：** [`docs/architecture/api-module-owners.md`](../../architecture/api-module-owners.md)

**Goal：** 按总图把膨胀的 `brep_core` 拆成 `brep_base` / 瘦 `brep_core` / `brep_mesh` / `brep_bool`。不改布尔算法、不改拓扑所有权、不打断 Viewer。

**进度（2026-08-26）：** P1–P4 的 CMake 与 `api/*.h` 已改进仓库。`include_boundaries` 已通过。本环境无法运行 MinGW `g++`/`ninja`（进程静默 exit 1），**请在本机** `cmake --build cmake-build-mingw-debug` 后全量 `ctest`。

**Architecture：** 第一期只改 CMake 源列表与少量 `api/*.h`；`kernel/src/**` 路径保持不动。INTERFACE 目标 `brep` 始终聚合全部内核库，`apps/viewer` 与 `tests/` 继续 `target_link_libraries(... brep)`。

**Tech Stack：** `cmake-build-mingw-debug`、MinGW、`BREP_BUILD_SHARED=ON`（`CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` 已开）、`CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_BINARY_DIR}/bin`、GoogleTest、`ctest -R include_boundaries`。

**建议节奏：** **一个阶段一个 PR**。P1 收益最大，必须先做。P5 可无限期不做。

---

## Global Constraints

- **无环：** `brep_bool` 不得 include `brep/feat/**`、`brep/Document.h`、`brep/Part.h`。`brep_mesh` 不得 include `brep/bool/**`。`brep_core` 不得 include `brep/bool/**` 或 `brep/Mesh.h` 的实现依赖（头文件 `Mesh.h` 可留在 include 树，但 `.cpp` 不在 core）。
- Viewer 生产代码（`bootstrap/` 除外）只 `#include "api/..."`。本计划不把 Qt / Hypodermic 拉进内核。
- **不改** `IBooleanEvaluator` 签名、不改 `Part` 构造注入、不改 Guid 布局、不改 R.3 观察指针。
- **不拆** `Intersect*.cpp`、不拆 `viewer_runtime`、不抽 `brep_snap` / `brep_spatial`。
- 风格：PascalCase 文件、4 空格 Allman、`m_` 成员。新 CMake 只加 target，不顺带格式化无关源文件。
- 每阶段禁止夹带：布尔数值修复、NURBS 显示、IoC 新服务。

### 构建与测试约定

```powershell
cd E:\brepkernelstudy
# 若未 configure：
# cmake -S . -B cmake-build-mingw-debug -G Ninja

cmake --build cmake-build-mingw-debug
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure
```

定向测试用 `-R` 正则（CTest 测试名，见 `tests/CMakeLists.txt`）。Windows 新 DLL 必须出现在 `cmake-build-mingw-debug/bin/`，与现有 `brep_core.dll` 同目录。

**回滚：** 每个阶段只动列出的文件；`git checkout --` 那些路径即可。P1 回滚三个 cmake + 删 `BrepBool.cmake`。

---

## 里程碑

| 阶段 | 交付 | 验收（必须全过） |
|------|------|------------------|
| **P0** | 分层文档 | 已完成 |
| **P1** | `brep_bool.dll` | core 源列表无 `src/bool/`；feat PUBLIC 链 bool；kernel ctest 绿；Viewer 能链上 |
| **P2** | `brep_mesh.dll` | core 无 Mesh/Cdt/LoopSample；tessellate/CDT 测试绿；mesh 不链 bool |
| **P3** | `brep_base.dll` | Guid/Log 不在 core；Viewer 日志与 ECS Guid 行为不变 |
| **P4** | `api/Base.h` + Modeling 收紧 + install | `include_boundaries`；owners/AGENTS 改为「当前已拆」 |
| **P5** | 目录与 target 对齐 | `Mesh.cpp` → `src/mesh/`；Guid/Log → `src/base/`（已做） |

---

## P0 — 文档（已完成）

- [x] [layering.md](../../architecture/layering.md)
- [x] ADR 0010；0002/0003/0007/0009 交叉引用
- [x] 本实施计划（详细版）
- [x] P4 已更新 `api-module-owners.md` 与 `AGENTS.md` 库名（本机 build / ctest 仍待跑）

---

## P1 — 抽出 `brep_bool`（优先）

### 为什么先做

`src/bool/*` 是 core 里体积最大、变更最勤的一块。抽出后改 Imprint 不再重编 Geometry/CDT。

### 现状源列表（从 `cmake/BrepCore.cmake` 迁出）

下列 **21 个** `.cpp` 从 `brep_core` 删除，进入 `brep_bool`：

```
kernel/src/bool/EvaluatorStub.cpp
kernel/src/bool/CompositeEvaluator.cpp
kernel/src/bool/Pipeline.cpp
kernel/src/bool/IntersectorRegistry.cpp
kernel/src/bool/IntersectionGraph.cpp
kernel/src/bool/TopologyCopy.cpp
kernel/src/bool/BooleanBuilder.cpp
kernel/src/bool/ImprintEngine.cpp
kernel/src/bool/SolidClassifier.cpp
kernel/src/bool/FaceSelector.cpp
kernel/src/bool/IntersectPlanePlane.cpp
kernel/src/bool/IntersectPlaneSphere.cpp
kernel/src/bool/IntersectSphereSphere.cpp
kernel/src/bool/IntersectPlaneCylinder.cpp
kernel/src/bool/IntersectSphereCylinder.cpp
kernel/src/bool/BoxRecognize.cpp
kernel/src/bool/PlanarRecognize.cpp
kernel/src/bool/SphereRecognize.cpp
kernel/src/bool/Classify.cpp
kernel/src/bool/Broadphase.cpp
```

**留下 core：** `FaceBvh.cpp`、`SnapQuery.cpp`、几何/拓扑/Guid/Log/Mesh（Mesh 到 P2 再走）。

**头文件不搬家：** 仍在 `kernel/include/brep/bool/`。

### Task P1.0 — 环依赖预检（只读）

在改 CMake 前确认源码已经符合 DAG（预期已满足；若失败先修 include 再拆库）：

```powershell
rg -n "#include `"brep/(feat|Document|Part|Mesh)" kernel/src/bool
rg -n "#include `"brep/bool" kernel/src/Geometry.cpp kernel/src/Topology.cpp kernel/src/Model.cpp kernel/src/Builder.cpp
```

- [ ] `bool/` 下无 feat/Document/Part/Mesh include
- [ ] Geometry/Topology/Model/Builder 无 `brep/bool` include

`Fuzzy.h` / `Polygon2d.h` 在 `kernel/internal/`，bool 与 mesh 都通过 `brep_kernel_include_dirs` 的 PRIVATE `internal/` 可见。**不要**把它们拷进 `api/`。

### Task P1.1 — 新建 `cmake/BrepBool.cmake`

按 `BrepFeat.cmake` 同构。完整模板：

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_bool ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/bool/EvaluatorStub.cpp
  ${BREP_KERNEL_DIR}/src/bool/CompositeEvaluator.cpp
  ${BREP_KERNEL_DIR}/src/bool/Pipeline.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectorRegistry.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectionGraph.cpp
  ${BREP_KERNEL_DIR}/src/bool/TopologyCopy.cpp
  ${BREP_KERNEL_DIR}/src/bool/BooleanBuilder.cpp
  ${BREP_KERNEL_DIR}/src/bool/ImprintEngine.cpp
  ${BREP_KERNEL_DIR}/src/bool/SolidClassifier.cpp
  ${BREP_KERNEL_DIR}/src/bool/FaceSelector.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlanePlane.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlaneSphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectSphereSphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlaneCylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectSphereCylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/BoxRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/PlanarRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/SphereRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/Classify.cpp
  ${BREP_KERNEL_DIR}/src/bool/Broadphase.cpp
)
brep_kernel_include_dirs(brep_bool)
target_link_libraries(brep_bool PUBLIC brep_core)
set_target_properties(brep_bool PROPERTIES OUTPUT_NAME brep_bool)

if(MSVC)
  target_compile_options(brep_bool PUBLIC /utf-8)
endif()
```

- [x] `cmake/BrepBool.cmake` 已加入仓库（**需本地 `cmake --build` 验收 DLL**）
- [x] **只** PUBLIC 链 `brep_core`（FaceBvh 符号在 core 里，Broadphase 需要）

### Task P1.2 — 改 `cmake/BrepCore.cmake`

- [x] 删除上表全部 `src/bool/*.cpp` 行
- [x] **保留** `spatial/FaceBvh.cpp`、`snap/SnapQuery.cpp`、Mesh/Guid/Log
- [x] `target_link_libraries(brep_core PUBLIC brep_eigen brep_boost_uuid brep_spdlog)` 本阶段不变

### Task P1.3 — 改 `cmake/BrepFeat.cmake`

`Part.cpp` / `BooleanFeature.cpp` 调用 `MakeDefaultBooleanEvaluator`（定义在 bool 库）。SHARED 下 feat 必须能解析该符号。

- [x] `target_link_libraries(brep_feat PUBLIC brep_core brep_bool)`

不要改成 PRIVATE bool：公开头 `Part.h` include `brep/bool/Evaluator.h`，使用 feat 的目标必须看见 bool 的 include/链接。

### Task P1.4 — 改 `cmake/BrepAll.cmake`

include **顺序**（被依赖者先）：

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/BrepCore.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepBool.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepFeat.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepIO.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepAsm.cmake)
```

- [x] `target_link_libraries(brep INTERFACE brep_io brep_asm brep_feat brep_bool brep_core)`
- [x] 增加 `Brep::brep_bool` ALIAS

```cmake
if(NOT TARGET Brep::brep_bool)
  add_library(Brep::brep_bool ALIAS brep_bool)
endif()
```

注释里的 DAG 改为：`core ← bool ← feat ← asm`；`io` 仍链 feat+asm。

### Task P1.5 — Install（可推迟到 P4，但 P1 若有人 `cmake --install` 会漏 DLL）

**推荐 P1 就改** `cmake/BrepInstall.cmake` 的 `_brep_install_targets`：

```
brep_eigen, brep_boost_uuid, brep_spdlog,
brep_core, brep_bool, brep_feat, brep_asm, brep_io, brep
```

顺序：被依赖者在前。P2/P3 再插入 mesh/base。

- [x] P1 已写入 install 列表（`brep_bool` 在 `brep_core` 之后）

### Task P1.6 — 不改这些

- `apps/viewer/CMakeLists.txt`（仍链 `brep`）
- `tests/CMakeLists.txt`（仍链 `brep`）
- `KernelServices.cpp` / Hypodermic 模块（工厂符号随 `brep` 传递）
- 任何 `kernel/src/bool/*.cpp` 算法

### Task P1.7 — 验证

```powershell
cmake --build cmake-build-mingw-debug --target brep_bool brep_core brep_feat brep
# 确认 bin 下出现 brep_bool.dll（MinGW 可能同时有 libbrep_bool.dll.a）

ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure -R "include_boundaries|boolean|Boolean|Imprint|IntersectionGraph|TopologyCopy|SolidClassifier|Broadphase|box_boolean|planar_boolean|sphere"
```

建议再跑一遍无 `-R` 的全量 kernel+examples（`smoke` / `parametric_smoke`）。然后：

```powershell
cmake --build cmake-build-mingw-debug --target brep_viewer
```

手动：空白文档 → 建 Box → Union（确认命令能加载 `brep_bool.dll`）。

**链接失败对照：**

| 症状 | 原因 | 修 |
|------|------|-----|
| `MakeDefaultBooleanEvaluator` unresolved（feat 或 Viewer） | feat 未 PUBLIC 链 bool，或 `brep` INTERFACE 漏 bool | P1.3 / P1.4 |
| `FaceBvh` unresolved（bool） | bool 未链 core | P1.1 |
| 启动缺 `brep_bool.dll` | 输出目录不是 `bin/` | 确认 `BrepLibType.cmake` 仍设置 RUNTIME_OUTPUT |
| core 仍编译 `Pipeline.cpp` | P1.2 没删干净 | 对 `BrepCore.cmake` 搜 `bool/` |

**增量编译抽查（Ninja）：** `touch kernel/src/bool/ImprintEngine.cpp` 后 rebuild，`Geometry.cpp` / `Cdt.cpp` **不应**出现在重编列表。

---

## P2 — 抽出 `brep_mesh`

**前置：** P1 已合入。

### 迁出源

```
kernel/src/Mesh.cpp
kernel/src/mesh/Cdt.cpp
kernel/src/mesh/LoopSample.cpp
```

`kernel/include/brep/Mesh.h` 不搬家。`api/Mesh.h` 仍 `#include "brep/Mesh.h"`。

### Task P2.0 — 环预检

```powershell
rg -n "#include `"brep/bool" kernel/src/Mesh.cpp kernel/src/mesh
rg -n "tessellate_|Tessellate" kernel/src/bool
```

- [ ] mesh 实现不 include bool
- [ ] bool 不调用 tessellate（显示网格 ≠ 布尔）

### Task P2.1 — `cmake/BrepMesh.cmake`

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_mesh ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/Mesh.cpp
  ${BREP_KERNEL_DIR}/src/mesh/Cdt.cpp
  ${BREP_KERNEL_DIR}/src/mesh/LoopSample.cpp
)
brep_kernel_include_dirs(brep_mesh)
target_link_libraries(brep_mesh PUBLIC brep_core)
set_target_properties(brep_mesh PROPERTIES OUTPUT_NAME brep_mesh)

if(MSVC)
  target_compile_options(brep_mesh PUBLIC /utf-8)
endif()
```

- [ ] **禁止** `target_link_libraries(brep_mesh ... brep_bool)`

### Task P2.2 — Core / All / Install

- [ ] `BrepCore.cmake` 删除上述 3 个 `.cpp`
- [ ] `BrepAll.cmake`：`BrepCore` → `BrepMesh` → `BrepBool` → `BrepFeat` …
- [ ] `brep` INTERFACE 增加 `brep_mesh`
- [ ] `Brep::brep_mesh` ALIAS
- [ ] `_brep_install_targets` 在 `brep_core` 之后插入 `brep_mesh`

`brep_feat` **不必**直接链 mesh（再生不 tessellate）。Viewer 经 `brep` 聚合拿到 tessellate。  
**`brep_io` 必须 PUBLIC 链 `brep_mesh`**：`BksCache.cpp` 调用 `TessellateBody` / `ExtractEdges`（拆库后不再从 core 带过来）。

### Task P2.3 — 验证

```powershell
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure -R "include_boundaries|tessellate|Cdt|LoopSample|extract_edges|Cdt"
cmake --build cmake-build-mingw-debug --target brep_viewer
```

`touch kernel/src/Mesh.cpp` 后 rebuild，`ImprintEngine.cpp` 不应重编。

若某测试 exe 将来改为只链 `brep_core`（窄链接），tessellate 用例必须改链 `brep_mesh`。**本阶段不要改测试链接行。**

---

## P3 — 抽出 `brep_base`

**前置：** P1+P2。

### 迁出源

```
kernel/src/Guid.cpp
kernel/src/Log.cpp
```

头文件仍在 `kernel/include/brep/Guid.h`、`Log.h`、`Math.h`（Math 现为头文件 + Eigen，无独立 `.cpp`）。

### Task P3.1 — `cmake/BrepBase.cmake`

第三方从 **core 挪到 base**（uuid/spdlog 只服务 Guid/Log；Eigen 也给 Math 用，base PUBLIC 链上后 core 传递即可）：

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_base ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/Guid.cpp
  ${BREP_KERNEL_DIR}/src/Log.cpp
)
brep_kernel_include_dirs(brep_base)
target_link_libraries(brep_base PUBLIC brep_eigen brep_boost_uuid brep_spdlog)
set_target_properties(brep_base PROPERTIES OUTPUT_NAME brep_base)

if(MSVC)
  target_compile_options(brep_base PUBLIC /utf-8)
endif()
```

### Task P3.2 — Core / All / Install

- [ ] `BrepCore.cmake` 删除 `Guid.cpp` `Log.cpp`
- [ ] `target_link_libraries(brep_core PUBLIC brep_base)`  
      **删除** core 上直接的 `brep_eigen brep_boost_uuid brep_spdlog`（改由 base PUBLIC 传递）。若 Geometry.cpp 只通过 `Math.h` 用 Eigen，传递足够。
- [ ] `BrepAll.cmake`：**最先** `include(BrepBase.cmake)`，再 Core → Mesh → Bool → Feat
- [ ] `brep` INTERFACE 增加 `brep_base`
- [ ] `Brep::brep_base` ALIAS
- [ ] install 列表最前（在 INTERFACE 第三方之后）：`brep_base` 再 `brep_core` …

最终 install 顺序：

```
brep_eigen, brep_boost_uuid, brep_spdlog,
brep_base, brep_core, brep_mesh, brep_bool, brep_feat, brep_asm, brep_io, brep
```

### Task P3.3 — Viewer 零改业务

- [ ] 不改 `ecs/Components.h` / `CreateBoxTool` 的 Guid 用法
- [ ] 不改 `InitLogging` 调用点
- [ ] 确认 `api/Core.h` 仍 include `Guid.h` `Log.h`（P4 再抽 `api/Base.h`）

### Task P3.4 — 验证

```powershell
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure
cmake --build cmake-build-mingw-debug --target brep_viewer
```

- [ ] 启动 Viewer：日志文件/控制台仍有 `BREP_INFO`（MainWindow / 命令）
- [ ] 建 Box 后属性面板能显示 Guid 字符串
- [ ] `bin/` 含 `brep_base.dll`

**若 Geometry 编译找不到 Eigen：** core 补回 `PUBLIC brep_eigen`（与 base 重复 PUBLIC 无害），不要把 Eigen 改成 PRIVATE。

---

## P4 — API 收紧与文档「当前」列

**前置：** P1–P3 DLL 已存在。本阶段允许改 Viewer **仅当** Modeling 收紧导致编译失败。

### Task P4.1 — `kernel/include/api/Base.h`（新建）

```cpp
#pragma once

// Graded public API — identity / math / logging (high stability).

#include "brep/Guid.h"
#include "brep/Log.h"
#include "brep/Math.h"
```

`kernel/include/api/Core.h`：

- [ ] `#include "api/Base.h"`
- [ ] 删除重复的 `Guid.h` / `Log.h` / `Math.h`（`Geometry.h` 若已带 Math 可只留 Base + 几何/拓扑）
- [ ] 保留 `Geometry.h` `IObject.h` `Material.h` `Model.h` `Plane.h` `Topology.h` `Types.h`

`include_boundaries` 规则：`api/*.h` 只能 include `brep/` 或 `api/`。`api/Base.h` 合法。

### Task P4.2 — 收紧 `api/Modeling.h`

现状第 21 行：`#include "brep/bool/Boolean.h"`（整份 Pipeline 伞头）。

- [ ] **删除**该行
- [ ] **不要**再 include 伞头。`Part.h` 已 include `brep/bool/Evaluator.h` → `Types.h`，Viewer 经 `api/Modeling.h` → `Part.h` 仍能见到 `BooleanOp` / `IBooleanEvaluator`

可选同时新增窄头 `kernel/include/api/Boolean.h`：

```cpp
#pragma once

#include "brep/bool/Evaluator.h"
```

供「只要布尔接口、不要特征」的 TU 使用。

**若 Viewer 编译失败**（某 TU 曾依赖伞头里的 `Pipeline.h` 等）：

1. 生产代码：改为 `api/Boolean.h` 或回到只需要的类型（不应需要 Pipeline）。
2. `apps/viewer/bootstrap/` 已直接 include `brep/bool/Evaluator.h`（边界脚本对 bootstrap 豁免）。可选改为 `api/Boolean.h`，**非必须**。
3. 禁止为了图省事把 `Boolean.h` 伞头加回 `Modeling.h`。

已知经 `Modeling.h` 使用 `boolean::` 的文件（收紧后应仍能编，因 `Part.h`）：

- `PropertyPanel.cpp` / `.h`、`ISceneService.h`、`SceneAdapter.*`、`World.h`、`document.cpp`、`BuiltinCommands.cpp`、若干 `commands/tools/*`、`apps/viewer/tests/Test*.cpp`

### Task P4.3 — 文档与边界脚本

- [ ] [api-module-owners.md](../../architecture/api-module-owners.md)：Guid/Log 所有者 `brep_base`；Mesh 所有者 `brep_mesh`；增补 `api/Base.h` / 可选 `api/Boolean.h`；Modeling 行去掉「仍 include 伞头」
- [ ] [AGENTS.md](../../../AGENTS.md)：项目结构改为已存在的库名列表
- [ ] [layering.md](../../architecture/layering.md) §9「现状代码对照」改成已拆库
- [ ] 若新增 `api/Boolean.h`：ADR 0002 决策列表补第五个聚合头（短修订即可）

### Task P4.4 — 验证

```powershell
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure -R include_boundaries
cmake --build cmake-build-mingw-debug --target brep_viewer viewer_adapter_tests
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure -R "viewer_"
```

全量 `ctest` 一次。

---

## P5 — 目录与 target 对齐（已做）

`git mv` + 改 CMake 路径，未改 `#include "brep/..."` 前缀、未改算法。

| 现在 | 结果 |
|------|------|
| `kernel/src/bool/*` | 保持（已按目录） |
| `kernel/src/Mesh.cpp` | `kernel/src/mesh/Mesh.cpp` |
| `kernel/src/Guid.cpp` `Log.cpp` | `kernel/src/base/` |

CMake：`BrepMesh.cmake` → `src/mesh/Mesh.cpp`；`BrepBase.cmake` → `src/base/Guid.cpp` `Log.cpp`。

---

## 每阶段执行清单（复制用）

1. 读本阶段「不改这些」；开独立分支（例如 `split-p1-brep-bool`）。
2. 只改本阶段列出的 cmake/头/文档。
3. `cmake --build cmake-build-mingw-debug`
4. 本阶段 `ctest -R …`，再全量。
5. 确认 `cmake-build-mingw-debug/bin/brep_*.dll` 名单。
6. 编 `brep_viewer`；P1/P3/P4 至少手动点一次建 Box。
7. PR 说明写清：无算法变更、新增哪个 DLL。

---

## 目标 CMake DAG（P3 完成后）

```text
brep_base (eigen, uuid, spdlog)
    ↑
brep_core          （几何/拓扑/Model/FaceBvh/SnapQuery）
    ↑
    ├── brep_mesh
    └── brep_bool
            ↑
        brep_feat
            ↑
        brep_asm    brep_io（io 另链 asm）

brep INTERFACE = io + asm + feat + bool + mesh + core + base
```

`viewer_adapter` → `brep` → 运行时加载上述全部 SHARED（Windows）。

---

## 风险

| 风险 | 缓解 |
|------|------|
| 漏 DLL，Viewer 启动即失败 | `BrepLibType.cmake` 统一 `bin/`；P1 起检查 `brep_bool.dll` |
| feat 未链 bool | P1.3；链接错误信息会点名 `MakeDefaultBooleanEvaluator` |
| P3 后 core 找不到 Eigen | core 再 PUBLIC `brep_eigen` |
| Modeling 收紧打破 Viewer | P4 单独 PR；依赖 `Part.h` 已带 Evaluator；失败再加 `api/Boolean.h` 而不是伞头 |
| install/find_package 漏库 | `_brep_install_targets` 与 `Brep::` ALIAS 同步 |
| 内部头 Fuzzy 两边用 | 保持 `brep_kernel_include_dirs` PRIVATE internal，不进 api |
| bootstrap 仍 include `brep/bool/` | 脚本已豁免；P4 可选迁 `api/Boolean.h` |

---

## 与其它计划的衔接（不要混进本 PR）

| 工作 | 文档 | 相对本计划 |
|------|------|------------|
| 布尔算法正确性 | ADR 0008 实施计划 | **并行可以**，但不要和 P1 同一 PR |
| NURBS 显示 tessellate | ADR 0009 | **P2 之后**往 `brep_mesh` 加源，不要加进 core/bool |
| NURBS 求交 | 未来 ADR | 只进 `brep_bool` |
| NURBS 建模 UI | ADR 0011（未立项） | 不在范围 |
| IoC 新服务 | ioc-governance | 禁止注册 Pipeline 内部类型 |

---

## 非本计划

- 拆 `viewer_runtime`、顶层 `apps/adapter/`、`Guid.dll` 产品化
- 改 `IBooleanEvaluator` / 去掉 `Part` 构造注入
- 每个 `Intersect*` 独立 target
