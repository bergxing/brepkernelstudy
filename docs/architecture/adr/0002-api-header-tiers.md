# ADR 0002: 内核公开头分级（api/*）

**状态**：已接受（第一刀）  
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
4. **本切片不做**：`internal/` 目录隔离、include 边界强制脚本；examples 可暂继续用 umbrella。

## 理由

- 聚合头成本低，立刻统一 Viewer 依赖面，为后续 internal 隔离铺路。
- 不移动现有 `brep/*.hpp`，避免大搬家与链接/IDE 风险。
- 不做 `api/all.hpp`，避免再造全量 umbrella。

## 后果

- Viewer TU 可能因聚合头略增编译依赖（相对窄 include）。
- examples 编译时若 include `brep/brep.hpp` 会看到 `#warning` / `#pragma message`。
