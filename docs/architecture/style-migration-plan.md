# C++ 风格迁移方案（Google Style + 项目覆盖）

**状态**：批次 1–26 已完成（2026-08-21）  
**日期**：2026-08-21 更新  
**规范来源**：`.cursor/skills/google-cpp-style/project-overrides.md`

---

## 已完成批次（1–17）

| # | 范围 | 脚本 |
|---|------|------|
| 1–8 | Part/Document/Topology/Geometry/commands/identity | `refactor_*_api.py` |
| 0 | 头文件大小写、`Evaluator.h`/`Pipeline.h`、viewer 源 PascalCase | 手工 + CMake |
| 9 | `Parameter::Kind/Value/UserDriven` | `refactor_param_struct_api.py` |
| 10 | Assembly/Mate/Occurrence + 方法 | `refactor_asm_api.py` |
| 11 | Sketch Constraint + 方法 | `refactor_sketch_api.py` |
| 12 | Material/Plane/Snap/Mesh opts/BooleanContext/TopologyRef/viewer struct | `refactor_kernel_dto_api.py` |
| 13 | Kernel 自由函数 PascalCase（主体） | `refactor_kernel_free_functions_api.py` |
| 14 | Viewer ECS/CommandContext/Camera 字段 | `refactor_kernel_dto_api.py` |
| 15 | Viewer 自由函数 + 类方法 | `refactor_viewer_api.py` + `refactor_viewer_class_methods_api.py` |
| 16 | Kernel 私有 `*_`→`m_`（Geometry/Model/Sketch/FaceBvh/Guid 等） | `refactor_kernel_private_members_api.py` |
| 17 | `project-overrides.md`、本方案初版 | — |

> 批次 1–17 完成的是 **对外类方法、核心 DTO 子集、viewer 主体**；**公开 struct 字段、局部 struct、源文件名、Math/Guid 工具 API** 仍在批次 18+。

---

## 批次 18–26（按顺序执行，已全部完成）

| # | 范围 | 脚本 | 验证目标 |
|---|------|------|----------|
| **18** | Kernel 自由函数遗漏：`ExtractProfile`、`Extrude`、`ClassifyPointInPrism`、`LastXlError`、`TriangulatePolygonWithHolesUnconstrained` | `refactor_batch18_kernel_free_functions.py` | `brep_core` `brep_feat` |
| **19** | `Mesh.h` DTO：`MeshVertex`/`TriangleMesh`/`EdgeMesh`/`CdtResult` 字段 PascalCase | `refactor_batch19_mesh_dto.py` | + viewer tessellation |
| **20** | Builder/ops DTO：`BoxSpec`/`SphereSpec`/`Profile2d`/`ExtrudeSpec`/`PlanarPrismSpec`/`Aabb` 字段 | `refactor_batch20_builder_ops_dto.py` | + examples/tests |
| **21** | Validate + IO 结果 struct：`ValidationIssue`/`ValidationReport`/`XlSaveResult`/… 字段与方法 | `refactor_batch21_validate_io_dto.py` | + xl/bks tests |
| **22** | Spatial：`FaceBvhNode`/`QueryStats`/`FaceBvh`/`Aabb` 字段与方法 | `refactor_batch22_spatial_api.py` | broadphase tests |
| **23** | Sketch 实体 struct：`SketchPoint`/`SketchLine`/`Constraint` 等字段 | `refactor_batch23_sketch_entity_dto.py` | sketch/solver tests |
| **24** | 源文件 PascalCase 重命名（git index 与 CMake 对齐） | `refactor_batch24_source_filenames.py` | 全量 kernel 链接 |
| **25** | `.cpp` 局部/匿名 struct 字段（如 `Builder.cpp` 的 `FaceBuild`） | `refactor_batch25_local_structs.py` | `brep_core` |
| **26** | `Guid` API、`Math.h` 私有成员、`Topology.h` 图字段、剩余 snake_case 源文件 git 索引 | `refactor_batch26_*.py` | 全量 kernel + viewer |

### 批次 18 明细

| 旧名 | 新名 | 主要头文件 |
|------|------|------------|
| `extract_profile` | `ExtractProfile` | `ops/Profile.h` |
| `extrude` | `Extrude` | `ops/Profile.h` |
| `classify_point_in_prism` | `ClassifyPointInPrism` | `bool/Classify.h` |
| `LastXl_error` | `LastXlError` | `io/XlDocument.h` |
| `TriangulatePolygonWithHoles_unconstrained` | `TriangulatePolygonWithHolesUnconstrained` | `mesh/Cdt.h` |

### 批次 19 明细

| struct | 字段映射 |
|--------|----------|
| `MeshVertex` | `position→Position`, `normal→Normal`, `uv→Uv` |
| `TriangleMesh` | `vertices→Vertices`, `indices→Indices` |
| `EdgeMesh` | `positions→Positions` |
| `CdtVertex`/`CdtTriangle`/`CdtResult` | 同上规则 |

### 批次 20 明细

| struct | 字段映射 |
|--------|----------|
| `BoxSpec` | `min→Min`, `max→Max`, `tolerance→Tolerance` |
| `SphereSpec` | `center→Center`, `radius→Radius`, `slices→Slices`, `stacks→Stacks`, `tolerance→Tolerance` |
| `Profile2d` | `outer→Outer`, `holes→Holes` |
| `ExtrudeSpec` | `profile→Profile`, `plane→Plane`, `distance→Distance`, `symmetric→Symmetric`, `tolerance→Tolerance`, `name→Name` |
| `PlanarPrismSpec` | 各几何字段 PascalCase |
| `Aabb` | `min→Min`, `max→Max` |

### 批次 21 明细

| struct | 字段/方法映射 |
|--------|----------------|
| `ValidationIssue` | `severity→Severity`, `where→Where`, `message→Message` |
| `ValidationReport` | `issues→Issues`, `ok()→Ok()`, `error()→Error()`, `warning()→Warning()` |
| `XlSaveResult`/`XlLoadResult` | `ok→Ok` 等 |
| `CacheSaveResult`/`CacheLoadResult` | 同上 |

### 批次 24 待重命名源文件（22 个，磁盘小写 → PascalCase）

```
kernel/src/builder.cpp          → Builder.cpp
kernel/src/document.cpp         → Document.cpp
kernel/src/dump.cpp             → Dump.cpp
kernel/src/geometry.cpp         → Geometry.cpp
kernel/src/guid.cpp             → Guid.cpp
kernel/src/log.cpp              → Log.cpp
kernel/src/mesh.cpp             → Mesh.cpp
kernel/src/model.cpp            → Model.cpp
kernel/src/part.cpp             → Part.cpp
kernel/src/topology.cpp         → Topology.cpp
kernel/src/validate.cpp         → Validate.cpp
kernel/src/asm/assembly.cpp     → Assembly.cpp
kernel/src/bool/broadphase.cpp  → Broadphase.cpp
kernel/src/bool/classify.cpp    → Classify.cpp
kernel/src/bool/pipeline.cpp    → Pipeline.cpp
kernel/src/feat/context.cpp     → Context.cpp
kernel/src/feat/regenerator.cpp → Regenerator.cpp
kernel/src/mesh/cdt.cpp         → Cdt.cpp
kernel/src/ops/extrude.cpp      → Extrude.cpp
kernel/src/param/parameter.cpp  → Parameter.cpp
kernel/src/sketch/sketch.cpp    → Sketch.cpp
kernel/src/solve2d/solver.cpp   → Solver.cpp
```

CMake（`cmake/BrepCore.cmake` 等）已使用 PascalCase；批次 24 仅修正 **git 跟踪名**（Windows 需 `git mv` 两步）。

### 批次 26 明细

| 范围 | 映射 |
|------|------|
| `Guid` | `generate→Generate`, `nil→Nil`, `from_string→FromString`, `from_bytes→FromBytes`, `to_string→ToString`, `bytes→Bytes` |
| `Math.h` 私有存储 | `Vector3d`/`Point3d`/`Point2d` 内 `data_→m_data` |
| `Topology.h` 图字段 | `Vertex::Point/Edges`, `Edge::Curve/V0/V1/T0/T1/Radial`, `CoEdge::Edge/Sense/Next/Prev/Partner/Pcurve/Loop`, `Loop::Face/First`, `Face::Surface/Sense/Loops`, `Shell::Faces/Closed`, `Body::Type/Shells` |
| 源文件 git 索引 | bool/feat/io/mesh/snap/spatial 剩余 snake_case → PascalCase（`refactor_batch26_git_index_fixup.py`） |

---

### 批次 25 局部 struct 示例

| 文件 | struct | 字段 |
|------|--------|------|
| `Builder.cpp` | `FaceBuild` | `origin→Origin`, `u_axis→UAxis`, `v_axis→VAxis`, `edge_idx→EdgeIdx`, `forward→Forward`, `uv→Uv` |
| `ops/Extrude.cpp` | `RingGeom` | `bottom_v→BottomV`, … |
| `mesh.cpp` | `ComplementMeshWelder` | `weld_tol→WeldTol` |

---

## 脚本索引

| 脚本 | 用途 |
|------|------|
| `refactor_batch18_kernel_free_functions.py` | 批次 18 |
| `refactor_batch19_mesh_dto.py` | 批次 19 |
| `refactor_batch20_builder_ops_dto.py` | 批次 20 |
| `refactor_batch21_validate_io_dto.py` | 批次 21 |
| `refactor_batch22_spatial_api.py` | 批次 22 |
| `refactor_batch23_sketch_entity_dto.py` | 批次 23 |
| `refactor_batch24_source_filenames.py` | 批次 24 |
| `refactor_batch25_local_structs.py` | 批次 25 |
| `refactor_batch26_guid_api.py` | 批次 26 — `Guid::Generate/FromString/ToString/Bytes` |
| `refactor_batch26_topology_fields.py` | 批次 26 — `Topology.h` 图字段 + 访问点 |
| `refactor_batch26_topology_fixup.py` | 批次 26 — 遗漏字段（`->T0`、`->Radial` 等） |
| `refactor_batch26_math_members.py` | 批次 26 — `Math.h` `data_→m_data` |
| `refactor_batch26_source_filenames.py` | 批次 26 — bool/feat/io 剩余源文件 |
| `refactor_batch26_git_index_fixup.py` | 批次 26 — Windows git index 对齐 |
| （历史）`refactor_kernel_dto_api.py` 等 | 批次 1–17 |

---

## 每批执行流程

```powershell
# 1. 跑脚本
python scripts/refactor_batchNN_*.py

# 2. 编译
cmake --build cmake-build-mingw-debug --target brep_core brep_feat brep_viewer -j 18

# 3. 测试
ctest -C Debug --output-on-failure
ctest -R include_boundaries
```

---

## 风险规则（已验证 + 新增）

1. `Guid` 与 `FeatureId` 均用 `.IsValid()`
2. 跳过 `third_party/`、`apps/viewer/i18n`
3. **struct 字段** PascalCase；**函数参数** camelCase
4. 批次 20 中 `ExtrudeSpec.name→Name` 与 `IObject::Name` 不同域，无冲突
5. `ValidationIssue::Severity` 枚举与字段 `Severity` 同名——字段用 `Severity`，枚举仍 `ValidationIssue::Severity`
6. **批次 20 脚本禁止改函数参数名**（参数保持 camelCase；仅 struct 字段 PascalCase）
7. `Aabb`/`BoxSpec` 字段 `Min`/`Max` 与 `std::min`/`std::max` 无冲突（用 `\b` 或 `spec.Min` 等限定）
8. **批次 26 Topology 脚本**：仅改 `Topology.h` 公开图字段；**局部变量**（`positions`/`outer_uv`/`first`/`uv`/`xyz`）与匿名 struct 字段（如 `LineSegment::edge`）保持 camelCase；成员访问用 `loop->First`/`c.Partner`，勿把局部 `first` 改成 `First`
9. **类 vs struct 成员**：`class` 实例成员 **`m_` + camelCase**；`struct`/DTO **PascalCase**；**禁止**把函数参数/局部变量改成 PascalCase；**禁止**类成员用 PascalCase 或无 `m_` 的 `snake_case_`

---

## 进度勾选

- [x] 批次 1–17
- [x] 批次 18 — Kernel 自由函数遗漏（脚本 + 手工收尾）
- [x] 批次 19 — Mesh DTO（`Mesh.h`/`Cdt.h` 字段 + 访问点）
- [x] 批次 20 — Builder/ops DTO（`BoxSpec`/`SphereSpec`/`Profile2d`/`ExtrudeSpec`/`PlanarPrismSpec`/`Aabb`）
- [x] 批次 21 — Validate/IO DTO（`ValidationReport`/`XlSaveResult` 等）
- [x] 批次 22 — Spatial API（`FaceBvh`/`Aabb`/`QueryStats`）
- [x] 批次 23 — Mesh 采样 + Sketch 实体 DTO（`FaceRegion`/`SampledRing`/`SketchPoint` 等）
- [x] 批次 24 — 源文件名 git 重命名（Windows 两步 `git mv`；`pipeline.cpp` 为 untracked 时 `git add`）
- [x] 批次 25 — 局部 struct（`FaceBuild`/`RingGeom`）
- [x] 批次 26 — Guid/Math/Topology 图字段 + 剩余源文件 git 索引
