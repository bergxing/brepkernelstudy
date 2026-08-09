# ADR 0002: 内核公开头分级（api/*）

**状态**：已接受  
**日期**：2026-08-09  

## 背景

Phase 3 后 Viewer 已不再使用 `brep/brep.hpp`，但仍直接 include 大量 `brep/*.hpp`。工程化计划 Phase 4 要求按稳定级别提供聚合头，并引导客户端改用分级 API。

## 决策

1. 在 `kernel/include/api/` 提供四个聚合头：
   - `api/core.hpp` — 几何 / 拓扑 / 模型 / 身份 / 材质 / 日志
   - `api/mesh.hpp` — 三角化与边提取
   - `api/modeling.hpp` — Document/Part、builder、feat、param、sketch
   - `api/persistence.hpp` — `.xl` / BKS cache IO
2. `apps/viewer/**`（含 Adapter 与 viewer tests）**只允许** `#include "api/..."`。
3. `brep/brep.hpp` 保留为兼容 umbrella，并带编译期废弃提示 + `[[deprecated]]` 哨兵。
4. **私有头**：实现细节放在 `kernel/internal/brep/internal/`，仅作为各 `brep_*` target 的 **PRIVATE** include；不得出现在 `kernel/include/` 下。
5. **边界检查**：`scripts/check_include_boundaries.py` 由 CMake 注册为 `ctest` 名 `include_boundaries`。
6. **类型归属文档**：[`api-module-owners.md`](api-module-owners.md)。
7. 不做 `api/all.hpp`（避免再造全量 umbrella）。examples 可暂继续用 `brep/brep.hpp`（会有废弃警告）。

## 理由

- 聚合头成本低，立刻统一 Viewer 依赖面。
- PRIVATE `kernel/internal` 满足「internal 不进 PUBLIC path」，无需一次性搬家现有 `brep/*.hpp`。
- 脚本检查可进 CI / 本地 `ctest`，比仅靠约定更可靠。

## 后果

- Viewer TU 可能因聚合头略增编译依赖。
- 新增内核私有头时应放在 `kernel/internal/`，并由脚本保证客户端不可见。
- 缺少 Python3 时 CMake 会警告并跳过 `include_boundaries` 测试。
