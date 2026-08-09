# ADR 0003: 动态库分层（SHARED）与 Viewer runtime 合并

**状态**：已接受  
**日期**：2026-08-09  

## 背景

工程化重构后逻辑分层已存在，但各模块仍是 **STATIC**，最终打进单一 exe。希望按依赖自上而下以 **动态库** 连接，便于独立替换/部署，并与 `docs/architecture` 描述一致。

## 决策

1. **默认 `BREP_BUILD_SHARED=ON`**（可用 `-DBREP_BUILD_SHARED=OFF` 回到静态库）。
2. **内核 DLL（无环）**：
   - `brep_core` ← `brep_feat` ← `brep_io`
   - `brep_feat` ← `brep_asm`；`brep_io` 另链 `brep_asm`（xl 加载调用 MateSolver）
   - 将 `document.cpp` / `part.cpp` 编入 **`brep_feat`**（Part 依赖特征实现），消除 core↔feat 链接环。
3. **Viewer DLL**：
   - `viewer_adapter` → 仅依赖 `brep`
   - `viewer_runtime` → 合并原 `viewer_scene` + `viewer_commands` + `viewer_render`（三者源码互引，无法安全拆成独立 DLL）
   - `viewer_ui` → 依赖 `viewer_runtime` + `viewer_adapter`
   - `brep_viewer.exe` → 仅链 `viewer_ui`
4. **同仓多 target**，不做多 Git 仓库；产物统一输出到 `${CMAKE_BINARY_DIR}/bin`，保证 Windows 能找到 DLL。
5. 保留 CMake **ALIAS**：`viewer_scene` / `viewer_commands` / `viewer_render` → `viewer_runtime`（兼容旧链接名）。

## 理由

- SHARED 要求 **有向无环** 依赖；原 STATIC 靠最终链接掩盖环依赖。
- 彻底拆开 scene/commands/render 需大规模接口重构；合并为 `viewer_runtime` 在交付形态上仍是独立动态库，成本可控。
- `part.cpp` 下沉到 feat 是内核侧最小改动，换取干净 DAG。

## 后果

- 运行目录需包含：`brep_*.dll`、`viewer_*.dll` 与 exe（以及 Qt/MinGW 运行时）。
- 源码目录仍可按 scene/commands/render 分子文件夹；**链接单元**以 `viewer_runtime` 为准。
- 文档与 ADR 同步描述该布局（见 engineering-refactoring-plan §3）。
