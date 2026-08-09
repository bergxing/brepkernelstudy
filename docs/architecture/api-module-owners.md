# 公开 API 类型归属（Phase 4）

客户端应通过 `api/*.hpp` 引入这些类型。`brep/internal/**`（位于 `kernel/internal/`，PRIVATE）不属于公开面。

| 聚合头 | 稳定级别 | 所有者模块 (CMake) | 主要公开类型 |
|--------|----------|-------------------|--------------|
| `api/core.hpp` | 高 | `brep_core` | `Point3d`, `Vector3d`, `Guid`, `IObject`, `ObjectKind`, `Model`, `Body` / 拓扑实体, `Material`, `Plane`, `RigidTransform`, 日志宏 |
| `api/mesh.hpp` | 中 | `brep_core` | `MeshVertex`, `TriangleMesh`, `EdgeMesh`, `tessellate_body`, `extract_edges` |
| `api/modeling.hpp` | 中 | `brep_feat`（含 Part/Document 实现） | `Document`, `Part`, `BoxSpec`, `BoxFeature`, `ExtrudeFeature`, `IFeature`, `FeatureId`, `FeatureTree`, `FeatureHistory`, `ParameterStore`, `Sketch`, `ConstraintSolver`, `Profile2d`, `ExtrudeSpec` |
| `api/persistence.hpp` | 低 | `brep_io` | `XlSaveResult`, `XlLoadResult`, `BodyMeshCache`, `load_xl` / `save_xl`, `load_bks_cache` / `save_bks_cache` |

## Viewer 动态库归属

| 库 | 内容 |
|----|------|
| `viewer_adapter` | `SceneAdapter`, `DocumentService` |
| `viewer_runtime` | ECS scene、commands/tools、Vulkan render、`DocumentSession`（原三分库合并） |
| `viewer_ui` | MainWindow / Home / 属性面板 / 菜单 |

## 未进入分级聚合、但仍在 `brep/` 公开树中的类型

以下仍可通过 `brep/...` 或 umbrella 访问（examples 兼容），**Viewer 不得直接 include**：

| 头文件 | 模块 | 说明 |
|--------|------|------|
| `brep/asm/assembly.hpp` | `brep_asm` | 装配；未来可升入 `api/assembly.hpp` |
| `brep/dump.hpp` / `validate.hpp` | `brep_core` | 调试 / 校验工具 |
| `brep/naming/topology_ref.hpp` | `brep_core` | 拓扑命名引用 |
| `brep/object_registry.hpp` | `brep_core` | 文档级 Guid 注册表 |
| `brep/brep.hpp` | （umbrella） | **已废弃** |

## 强制手段

- 静态检查：[`scripts/check_include_boundaries.py`](../../scripts/check_include_boundaries.py)（`ctest -R include_boundaries`）
- CMake：`kernel/include` = PUBLIC；`kernel/internal` = PRIVATE（见 `cmake/BrepKernelIncludes.cmake`）
- 默认 SHARED：`BREP_BUILD_SHARED=ON`（见 ADR 0003）
