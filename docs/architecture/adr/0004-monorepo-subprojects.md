# ADR 0004: 同仓多子工程

**状态**：已接受（一期 + 二期已落地）  
**日期**：2026-08-09  
**前置**：ADR 0003（SHARED 动态库分层）已落地  

## 背景

当前已是「同仓 + 多 SHARED target」，但仍是**根目录单一 `project(brep-kernel)`**。  
升级为 **同仓多子工程**：内核与 Viewer 各自有 CMake 工程边界，默认可从仓库根一键构建；内核可单独 `-S kernel` 配置。

约束：

- ❌ 不拆多 Git 仓库  
- ✅ 保持 SHARED 依赖 DAG（ADR 0003）  
- ✅ Viewer 继续通过 `api/*` + `viewer_adapter` 使用内核  

## 已确认决策（2026-08-09）

1. **采纳「A 为主、B 二期」**  
   - **一期**：嵌套 `project()` + 根 `add_subdirectory` 聚合。  
   - **二期**：`install(EXPORT)` / `BrepConfig.cmake`，Viewer 可 `find_package(Brep)`。  
2. **`examples/` 归属 `kernel` 子工程**（源码路径可仍在仓库根 `examples/`，由 kernel 的 CMake 添加目标）。  
3. **不做 Superbuild / ExternalProject**。  

## 目标

1. **`kernel/` 可独立构建**：`brep_*.dll` + examples + `tests/kernel`。  
2. **根工程变薄**：第三方探测（或委托）+ `add_subdirectory(kernel)` + 可选 Viewer。  
3. **`apps/viewer/` 为 `project(brep_viewer)`**：子目录消费 `brep`；亦可独立 `find_package(Brep)`。  
4. **日常路径不变**：开仓库根一次 configure → exe + 全部 DLL。  

## 工程边界

```text
brepkernelstudy/
├── CMakeLists.txt              # project(brepkernelstudy) — 薄聚合
├── cmake/
│   ├── BrepInstall.cmake       # install(EXPORT) + Config
│   ├── BrepConfig.cmake.in
│   └── …
├── kernel/
│   └── CMakeLists.txt          # project(brep_kernel)
├── apps/viewer/
│   └── CMakeLists.txt          # project(brep_viewer)
├── examples/                   # 由 kernel 子工程 add_executable
├── tests/kernel/               # 由 kernel 子工程接入
└── third_party/
```

构建时依赖：

```text
根聚合
  ├─ add_subdirectory(kernel)       → brep_* + examples + kernel tests + install rules
  └─ add_subdirectory(apps/viewer)  → viewer_* + brep_viewer（需 Qt）
```

单独构建内核：

```bash
cmake -S kernel -B build-kernel -DBREP_BUILD_SHARED=ON
cmake --build build-kernel
ctest --test-dir build-kernel
cmake --install build-kernel --prefix install-kernel
```

单独构建 Viewer（对已安装的 Brep）：

```bash
cmake -S apps/viewer -B build-viewer ^
  -DCMAKE_PREFIX_PATH="C:/Qt6/6.8.3/mingw_64;install-kernel"
cmake --build build-viewer
```

或对构建树（不 install）：

```bash
cmake -S apps/viewer -B build-viewer -DBrep_DIR=build-kernel
```

导出目标：`Brep::brep` / `Brep::brep_core` / …；Config 额外提供无命名 `brep` 别名。

## 非目标

- 不改变运行时行为与 `api/*` 边界  
- 不把 `viewer_runtime` 再拆成多个 DLL  
- 不把 Viewer 自身做成可 `find_package` 的安装包（仅内核）  

## 验收

- [x] 仓库根 configure + build：`ctest` 全绿；`brep_viewer` 可运行（冒烟仍见工程化计划附录 A）  
- [x] `cmake -S kernel -B …` 独立编出内核 DLL 并跑通 examples / kernel tests  
- [x] 工程化计划目录树 / 依赖图与本文一致  
- [x] `cmake --install` 产出 `lib/cmake/Brep/`；Viewer 可经 `find_package(Brep)` 独立配置并链接  
