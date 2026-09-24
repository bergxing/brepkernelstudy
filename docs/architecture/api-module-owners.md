# 公开 API 类型归属

客户端应通过 `api/*.h` 引入这些类型。`brep/internal/**`（位于 `kernel/internal/`，PRIVATE）不属于公开面。

**现行总图：** [layering.md](layering.md)。CMake 已按 ADR 0010 拆库（本机需重新 build 才能得到新 DLL）。

| 聚合头 | 稳定级别 | 所有者模块 (CMake) | 主要公开类型 |
|--------|----------|-------------------|--------------|
| `api/Base.h` | 高 | `brep_base` | `Guid`、日志宏、`Point3d` / `Math` |
| `api/Core.h` | 高 | `brep_core`（含 `api/Base.h`） | `IObject`、`ObjectKind`、`Model`、`Body` / 拓扑实体、`Material`、`Plane`、`RigidTransform` |
| `api/Mesh.h` | 中 | `brep_mesh` | `MeshVertex`、`TriangleMesh`、`EdgeMesh`、`tessellate_body`、`extract_edges` |
| `api/Boolean.h` | 中 | `brep_bool` | `IBooleanEvaluator`、`BooleanOp`、工厂 |
| `api/Modeling.h` | 中 | `brep_feat`（含 Part/Document 实现） | `Document`、`Part`、特征 / 草图 / 参数；经 `api/Boolean.h` 再导出 evaluator |
| `api/Persistence.h` | 低 | `brep_io` | `XlSaveResult`、`XlLoadResult`、`BodyMeshCache`、`load_xl` / `save_xl`、`load_bks_cache` / `save_bks_cache` |

## Viewer 动态库归属

| 库 | 内容 |
|----|------|
| `viewer_adapter` | `ISceneService` / `IDocumentService` 及实现（`SceneAdapter`, `DocumentService`） |
| `viewer_runtime` | ECS scene、commands/tools、Vulkan render（ADR 0003：不拆成三个 SHARED） |
| `viewer_bootstrap` | Hypodermic Composition Root（STATIC，ADR 0007） |
| `viewer_ui` | MainWindow / Home / 属性面板 / 菜单 |

## 未进入分级聚合、但仍在 `brep/` 公开树中的类型

以下仍可通过 `brep/...` 或 umbrella 访问（examples 兼容），**Viewer 不得直接 include**：

| 头文件 | 模块 | 说明 |
|--------|------|------|
| `brep/asm/Assembly.h` | `brep_asm` | 装配；未来可升入 `api/Assembly.h` |
| `brep/Dump.h` / `Validate.h` | `brep_core` | 调试 / 校验工具 |
| `brep/naming/TopologyRef.h` | `brep_core` | 拓扑命名引用 |
| `brep/ObjectRegistry.h` | `brep_core` | 文档级 Guid 注册表 |
| `brep/bool/Boolean.h` | `brep_bool` | Pipeline 伞头；examples / kernel 测试用 |
| `brep/Brep.h` | （umbrella） | **已废弃** |

## 强制手段

- 静态检查：[`scripts/check_include_boundaries.py`](../../scripts/check_include_boundaries.py)（`ctest -R include_boundaries`）
- CMake：`kernel/include` = PUBLIC；`kernel/internal` = PRIVATE（见 `cmake/BrepKernelIncludes.cmake`）
- 默认 SHARED：`BREP_BUILD_SHARED=ON`（见 ADR 0003）
