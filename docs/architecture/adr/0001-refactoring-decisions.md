# ADR 0001: 工程化重构关键决策

**状态**：已接受  
**日期**：2026-08-08  

## 背景

`brepkernelstudy` 内核与 Viewer 同仓，存在单库、大文件、Viewer 直接依赖内核具体类型等问题。详见 [engineering-refactoring-plan.md](../engineering-refactoring-plan.md)。

## 决策

1. **Adapter 位置**：`apps/viewer/adapter/`，命名空间 `brep::viewer::adapter`，CMake target `viewer_adapter`。
2. **测试框架**：GoogleTest（CMake FetchContent）。
3. **内核目录**：Phase 1 将 `include/`、`src/` 迁入 `kernel/include/`、`kernel/src/`。
4. **实施顺序**：Phase 0 → **2** → **1** → 3 → 4（先拆 Viewer 大文件，再 CMake/内核搬迁）。

## 理由

- Viewer 专用 Adapter 避免过早抽象；CLI 出现后再提炼公共层。
- GTest 与 CMake/CI 生态成熟，便于 `gtest_discover_tests`。
- 先 Phase 2 减少与 `kernel/` 物理搬迁的合并冲突。

## 后果

- Phase 2 新增 `apps/viewer/ui/` 等子目录，MainWindow 实现分文件。
- 根 CMake 增加 `tests/`，`-DBREP_BUILD_TESTS=OFF` 可关闭。
