# 三次贝塞尔曲线 — 实现任务拆分

> **设计规格：** [`docs/superpowers/specs/2026-08-27-bezier-curve-command-design.md`](../specs/2026-08-27-bezier-curve-command-design.md)（**交互以规格 §4 为准**，2026-09-16 已改为渐进次数：确定=下一步，第 4 点才退出）  
> **治理：** [`docs/architecture/viewer-adapter-governance.md`](../../architecture/viewer-adapter-governance.md)（禁止 `BezierSpecFor`；`BezierSpec` 进 `PrimitiveSpec`）  
> **风格：** PascalCase API、camelCase 参数、`m_` 成员、4 空格 Allman；Viewer 只 `#include "api/..."`  
> **日期：** 2026-08-27

**Goal：** 落地 `part.create_bezier`：最多 4 个 CV 的有理贝塞尔 Wire（2 点直线 / 3 点抛物线 / 4 点三次）→ 线框显示 → 特征撤销 / `.xl` → Copy 走 `SpecFor`。

**非目标（禁止夹带）：** 完整 `NurbsCurve` 节点算法、Pen 多段、草图 Bezier、扫掠/拉伸该曲线、抽壳、NURBS 求交、`ISceneService` 全量改名、DuplicateBody/Hollow。

---

## 0. 开工前必须知道的缺口

当前内核 **几乎只有 Solid**：`ExtractEdges` / `TessellateBody` / `ValidateBody` 都只走 `Body::Shells` → Face → Loop。`SampleEdgeXyz` 只认 `Line`/`Circle`，`TopologyCopy` 拷贝曲线 `default` 返回 nullptr。

因此 **Bz2 的 Wire 载体是本功能的硬前置**，不能只加 `BezierCurve::Eval` 就接 Viewer。

建议 Wire 挂边方式（选它，后面任务都按此写）：

```cpp
// Topology.h  Body
std::vector<Edge*> WireEdges;  // 仅 BodyType::Wire 使用；Solid 保持空
```

不要造无面的假 Face 来骗 ExtractEdges。

---

## 里程碑与 PR 切片

| PR | 里程碑 | 可独立合并 | 验收命令（本机 MinGW） |
|----|--------|------------|------------------------|
| 1 | **Bz1** 求值 | 是 | `ctest -R BezierCurve --output-on-failure` |
| 2 | **Bz2** Wire 拓扑 + 校验 + 采样显示 | 是（无 UI） | `ctest -R "BezierWire\|ExtractEdges\|Validate"` |
| 3 | **Bz3** 特征 / History / `.xl` | 是 | `ctest -R "BezierFeature\|Xl"` |
| 4 | **Bz4** 命令 + 菜单 + i18n | 依赖 Bz3 | 手动四点创建；中英提示 |
| 5 | **Bz5** Adapter `SpecFor` + CopyTool | 依赖 Bz3 | 复制平移；`TestSceneAdapter` |
| 6 | **Bz6**（可选）选中显示控制网 | 依赖 Bz4 | 选中 Wire 见 CV 折线 |

每 PR：`cmake --build cmake-build-mingw-debug` 后跑上表 `ctest`；全量 `ctest -C Debug --output-on-failure` 在 Bz3 和 Bz5 各做一次。

---

## Bz1 — 精确曲线求值（`brep_core`）

### Bz1.1 `CurveKind::Bezier` + `BezierSpec`

**Files：** `kernel/include/brep/Types.h`、`kernel/include/brep/feat/PrimitiveSpecs.h`

- [ ] `enum class CurveKind` 增加 `Bezier`（放在 `Circle` 与 `Nurbs` 之间）。
- [ ] `struct BezierSpec { Point3d P0, P1, P2, P3; double Tolerance{1e-7}; std::string Name{"bezier"}; }` 字段 **PascalCase**。
- [ ] `using PrimitiveSpec = std::variant<BoxSpec, SphereSpec, BezierSpec>;`  
  （所有 `std::get_if<BoxSpec>` / visit 编译点必须补 Bezier 臂或 `if constexpr`，见 Bz1.5。）

### Bz1.2 `BezierCurve`

**Files：** `kernel/include/brep/Geometry.h`、`kernel/src/Geometry.cpp`

- [ ] `class BezierCurve final : public Curve`：`Kind()==Bezier`；`Domain()=={0,1}`。
- [ ] `Eval(t)`：三次 Bernstein（规格 §5.1）；`t` 夹紧到 `[0,1]`。
- [ ] `Tangent(t)`：Bernstein 导函数；`t=0/1` 退化（P0=P1 等）时回退 `P3-P0` 或单位 X，**禁止 NaN**。
- [ ] 访问器 `P0()…P3()`；`SetControlPoints` 或四个 `SetP*`（供以后 `ApplyTransform`）。
- [ ] `Model::MakeBezier(BezierSpec spec)` → 拥有 `unique_ptr<BezierCurve>`，与 `MakeLine` 相同池。

### Bz1.3 采样折线（无拓扑）

**Files：** 建议 `kernel/include/brep/mesh/BezierSample.h` + `kernel/src/mesh/BezierSample.cpp`（`brep_mesh`），或先放 `Geometry.cpp` 若不想 Bz1 链 mesh。

**推荐：** 自由函数放 **core**（只依赖 `BezierCurve`），mesh 再调用：

```cpp
[[nodiscard]] std::vector<Point3d> SampleBezierPolyline(
    const BezierCurve& curve, int uniformSegments = 32);
```

- [ ] 均匀 \(N=32\)：`Eval(i/N)`，点数 33。
- [ ] （可同 PR 或 Bz2）弦高细分：`SampleBezierPolyline(curve, TessellationOptions)`，中点偏离 `LinearDeflection` 则拆，段数上限 128。

### Bz1.4 测试

**Files：** `tests/kernel/TestBezierCurve.cpp`、`tests/CMakeLists.txt`（新 `brep_test_bezier_curve`，`link brep`）

- [ ] `Eval(0)==P0`，`Eval(1)==P3`。
- [ ] 标准数值例：P0=(0,0,0), P1=(0,1,0), P2=(1,1,0), P3=(1,0,0)，`t=0.5` 与手算 Bernstein 比 `<1e-12`。
- [ ] 四点共线：`Eval(t)` 在线段上（叉积范数 `<1e-9`）。
- [ ] `SampleBezierPolyline` 点数、端点、相邻弦长 > 0。
- [ ] `Tangent` 在非退化端点与 `P1-P0` / `P3-P2` 同向。

### Bz1.5 编译修复（variant 加臂）

**Files：** 凡 `visit` / `get_if` `PrimitiveSpec` 处（当前主要是 `CopyTool.cpp`）

- [ ] CopyTool：Bezier 臂 **先跳过或 `static_assert` 未处理** 亦可，但必须能编译。推荐 Bz1 让 Copy 忽略 Bezier（`continue`），Bz5 再接平移。
- [ ] 若有其它 `std::variant<BoxSpec, SphereSpec>` 手写类型，改为 `PrimitiveSpec`。

**Bz1 DoD：** 不启动 Viewer；`ctest -R bezier_curve` 绿；`brep_core.dll` 链过。

---

## Bz2 — Wire 体、校验、显示采样

### Bz2.1 `Body::WireEdges`

**Files：** `kernel/include/brep/Topology.h`、`Dump.cpp`（可选列出 WireEdges）、`TestModelRemoveBody` 若假设 Shells 非空则放宽

- [ ] `Body` 增加 `std::vector<Edge*> WireEdges`。
- [ ] 注释：Solid/Sheet **禁止**往这里塞边；Wire **可以** `Shells.empty()`。

### Bz2.2 `MakeBezierWire`

**Files：** `kernel/include/brep/build/PrimitiveBuild.h`、`kernel/src/Builder.cpp`、`cmake/BrepCore.cmake`（已含 Builder）

- [ ] `Body* MakeBezierWire(Model& model, const BezierSpec& spec);`
- [ ] `MakePoint`×2（P0/P3）→ `MakeVertex`×2 → `MakeBezier` → `MakeEdge(curve, v0, v1, 0, 1, spec.Tolerance)` → `AttachEdgeToVertices`。
- [ ] `MakeBody(BodyType::Wire, spec.Name)`，`WireEdges.push_back(edge)`。
- [ ] **不要**创建 Face/Shell。

### Bz2.3 `ValidateBody` 分类型

**Files：** `kernel/src/Validate.cpp`、`kernel/include/brep/Validate.h`（注释）

- [ ] `BodyType::Wire`：`WireEdges.size()>=1`；每条边有 Curve、V0/V1；**不**报 `no shells`。
- [ ] Solid：保持现有「必须有 Shell/Face」。
- [ ] 测试：合法 Bezier Wire `ValidateBody.Ok()`；空 WireEdges 报错。

### Bz2.4 `SampleEdgeXyz` 支持 Bezier

**Files：** `kernel/src/mesh/LoopSample.cpp`、`LoopSample.h`

- [ ] `CurveKind::Bezier`：按 `T0/T1` 与 `BezierSample`（均匀 32 或弦高）采样；**禁止** `throw unsupported`。
- [ ] 增加 `SampleEdgeXyz(const Edge& edge, …)`（Sense=Forward），供无 CoEdge 的 Wire 使用。  
  或 ExtractEdges 为 Wire 造一个栈上 dummy CoEdge（不推荐，易脏）。

### Bz2.5 `ExtractEdges` / `TessellateBody`

**Files：** `kernel/src/mesh/Mesh.cpp`、`tests/kernel/TestExtractEdges.cpp`（或新 `TestBezierWire.cpp`）

- [ ] `ExtractEdges`：若 `body.Type==Wire`，遍历 `WireEdges`，采样折线写入 `EdgeMesh`（成对 Positions）。
- [ ] `TessellateBody`：Wire → 空 `TriangleMesh`（不 Warn 刷屏；可 Info 一次）。
- [ ] 测试：Bezier Wire `ExtractEdges` 段数 ≥ 32，折线端点 ≈ P0/P3；`faces` 空。

### Bz2.6 TopologyCopy（为以后 Copy 结果体；本里程碑可做最小）

**Files：** `kernel/src/bool/TopologyCopy.cpp`

- [ ] `CopyCurve`：`case CurveKind::Bezier` → `MakeBezier` 拷 4 点。
- [ ] `CopyBodySubgraph`：拷贝 `WireEdges`（与 Shell 循环并列）。  
  若不做，Bz5 体素 Copy 仍可用 `BezierSpec`，**融合体 DuplicateBody 遇 Bezier Wire 会失败**——在 Bz2 做完可避免以后踩坑。

**Bz2 DoD：** `MakeBezierWire` + Validate + ExtractEdges 单测绿；Viewer 仍无命令。

---

## Bz3 — 特征、撤销、存盘

### Bz3.1 `BezierCurveFeature`

**Files：** 新 `kernel/include/brep/feat/BezierCurveFeature.h`、`kernel/src/feat/BezierCurveFeature.cpp`、`cmake/BrepFeat.cmake`

- [ ] 对齐 `SphereFeature`：`Id/TypeName=="Bezier"/Status/Suppressed/BodyGuid/DisplayName`。
- [ ] 存 4 点（或 4 个 `ParameterId` 若要可编辑；**v1 允许点存在 feature 上不进 ParameterStore**，与 Box 的 LWH 不同，减少参数爆炸）。
- [ ] `Create(store, spec)`；`ToSpec()`；`ToPrimitiveSpec` → `BezierSpec`。
- [ ] `Rebuild`：若已有 body Guid 则按 `RebuildSphereBody` 同类路径替换/重建 Wire；`RegisterBody`。

### Bz3.2 `Part::AddBezier` + 删除/再生

**Files：** `Part.h`、`Part.cpp`

- [ ] `Body* AddBezier(const BezierSpec& spec = {});` → `AppendFeature` + `BodyForFeature`。
- [ ] `RemoveFeature` / `PurgeUnreferenced*` 能摘掉 Wire 拓扑（与现有 RemoveBody 路径一致；补测池大小不泄漏）。
- [ ] `RebuildBezierBody(Guid keepGuid, spec)` 如需要（对标 `RebuildSphereBody`）。

### Bz3.3 `FeatureHistory`

**Files：** `FeatureHistory.h`、`FeatureHistory.cpp`

- [ ] `FeatureTransaction` 增加 `BezierSpec Bezier{}`（**禁止**复用 `Box` 四个角存 CV）。
- [ ] `ApplyForward`：`FeatureType=="Bezier"` → `BezierCurveFeature::Create` + Append + Regen。
- [ ] Undo = `RemoveFeature`（现有 Append 反向已够）。

### Bz3.4 `.xl`

**Files：** `kernel/src/io/XlDocument.cpp`、`kernel/include/brep/io/XlDocument.h` 注释

- [ ] `FeatType::Bezier = 7`（`CopiedBody` 已占用 6；不要占用 1–6）。
- [ ] 写：Guid、Name、Suppressed、P0–P3（各 xyz）、BodyGuid、Tolerance。
- [ ] 读：重建 feature，进树后 `Regenerate`。
- [ ] 旧文件无 type 7 仍能打开。
- [ ] 测试：`SaveXl`/`LoadXl` 后 `FindBody` + `Spec` 四点一致。  
  （扩现有 xl 测试或 `tests/kernel/TestBezierFeature.cpp`。）

### Bz3.5 `api/Modeling.h`

- [ ] `#include "brep/feat/BezierCurveFeature.h"`（与 Sphere 相同，Viewer 可经 Modeling 见到类型，但命令仍只调 `Part`/`ISceneService`）。

### Bz3.6 测试

**Files：** `tests/kernel/TestBezierFeature.cpp`

- [ ] AddBezier → 1 个 Wire body，`Type==Wire`，`WireEdges.size()==1`。
- [ ] Undo/Redo 特征历史（经 `FeatureHistory::Undo` 或 `Part` API）。
- [ ] `.xl` 往返。
- [ ] `ToPrimitiveSpec` 命中 `BezierSpec`。

**Bz3 DoD：** 无 Qt；内核测试绿。

---

## Bz4 — Viewer 命令与绘制预览

### Bz4.1 Adapter 创建入口

**Files：** `ISceneService.h`、`SceneAdapter.*`、`TestSceneAdapter.cpp`

治理允许 **一种** 新创建函数，或直接做 `AddPrimitive`：

**推荐（少一次返工）：**

```cpp
Body* AddPrimitive(const PrimitiveSpec& spec);
void RecordAppendPrimitive(feat::FeatureId id, PrimitiveSpec undo);
```

- [ ] `visit`：Box → 现 `AddBox`+`record_append_feature`；Sphere 同理；Bezier → `AddBezier` + History `FeatureType Bezier`。
- [ ] 过渡：保留 `AddBox`/`AddSphere` 给旧工具；CreateBezierTool **只调** `AddPrimitive`。
- [ ] **禁止** `bezier_spec_for`。
- [ ] `object_for_feature`：`type_name=="Bezier"`，不必塞 BoxParams。
- [ ] 测试：`AddPrimitive(BezierSpec{…})` + `SpecFor` 四点。

若本 PR 不做 `AddPrimitive` 全集，最低限度：`AddBezier` + `record_append_bezier`，并在治理欠账列表注明 P4 收敛。**仍禁止 SpecFor 分叉。**

### Bz4.2 `CreateBezierTool`

**Files：** 新 `apps/viewer/commands/tools/CreateBezierTool.h/.cpp`、`apps/viewer/CMakeLists.txt`（`viewer_runtime` 源列表）

- [ ] `id()` = `part.create_bezier`；`allows_viewport_selection()==false`。
- [ ] `m_step` 0..3；`m_points[4]`；`prompt()` 用 `translate("CreateBezierTool", …)`（规格 §4.1 英文源文）。
- [ ] `pick_point` = `AccuSnap::resolve`（抄 `CreateSphereTool`）。
- [ ] 左键：距离 `<1e-6` 拒收；第四点 `commit_bezier`。
- [ ] `on_key_press`：`Key_Backspace` 退点；step0 再退 → `on_cancel`。
- [ ] `on_mouse_move`：按规格 §4.2 组 `EdgeMesh`（曲线采样 + 控制多边形 + `make_point_marker`）；`SetPreviewEdges`。
- [ ] `commit`：`BezierSpec` → `AddPrimitive`/`AddBezier` → `mesh_for_body`（edges 非空、faces 空）→ `create_body_renderable` → History undo/redo 与球相同（`undo_feature` + `sync_part_bodies`）。
- [ ] 失败：无文档 / 无 Part / `Add*` 失败；`Cancelled create bezier`。

预览采样：调用内核 `SampleBezierPolyline`（经 `api/Mesh.h` 或小函数放 Modeling）。**不要**在 Tool 里复制一份 Bernstein 还与内核不一致——把采样放进 `api/Mesh.h` 导出或 Tool include Modeling 后调 `SampleBezierPolyline`。

若 `SampleBezierPolyline` 在 `brep/mesh` 且 Viewer 只能 `api/Mesh.h`：

- [ ] `api/Mesh.h` 声明 `SampleBezierPolyline`（或 `SampleCurvePolyline`）。

### Bz4.3 注册命令

**Files：** `BuiltinCommands.cpp`

- [ ] `CreateBezierCommand`：`kind==Interactive`，`title=="Create Bezier Curve (interactive)"`，`can_execute` 与球相同，`make_tool` → `CreateBezierTool`。
- [ ] `register_builtin_commands`：`add<CreateBezierCommand>`。

### Bz4.4 菜单 / 工具栏 / 翻译

**Files：** `ui/MainWindowMenus.cpp`、`i18n/xcad_zh_CN.ts`、`PropertyPanel`（仅类型名，可不改）

- [ ] Modeling 菜单：`Create Be&zier Curve…`（避免与 Box 的 `&B` 冲突），objectName `act_model_bezier`。
- [ ] Tooltip 源文见规格 §3。
- [ ] `retranslate_ui` 同步 `set_act` / `set_tip`。
- [ ] 可选工具栏按钮（无图标可用文字 `Bezier`）。
- [ ] `xcad_zh_CN.ts`：`CreateBezierTool` 全部 `prompt`/错误串；`MainWindow` 菜单项。
- [ ] 语言切换时 `has_active_tool` 刷新状态栏（已有则无需再改）。

### Bz4.5 ECS / 渲染

- [ ] 确认 `create_body_renderable` 接受空 `TriangleMesh` + 非空 `EdgeMesh`（现逻辑应已支持）。
- [ ] 若 Vulkan 跳过无三角实体导致 **边也不画**：修 `VulkanDraw` / uploader「仅有 edges 仍上传」。**这是 Bz4 阻塞项，发现即修，不要静默。**

### Bz4.6 布尔误用（文档级，可不改代码）

- [ ] 若布尔命令在「两选」含 Wire 时失败，保持现有失败即可；不要在本 PR 做曲线布尔。

**Bz4 DoD：** 视口四点生成曲线；撤销消失；中英提示；不跑完整 NURBS。

---

## Bz5 — SpecFor 与 Copy

### Bz5.1 `SpecFor` / `ToPrimitiveSpec`

- [ ] `BezierCurveFeature::ToPrimitiveSpec` 已在 Bz3；`SceneAdapter::SpecFor` 无需改虚表（已调 `IFeature::ToPrimitiveSpec`）。
- [ ] `TestSceneAdapter.SpecForBezier`：Add 后 `get_if<BezierSpec>` 四点。

### Bz5.2 CopyTool

**Files：** `CopyTool.cpp`

- [ ] `visit`：`BezierSpec` → 平移 P0–P3（`+= offset`）→ `AddPrimitive`/`AddBezier` + record。
- [ ] 预览：`SampleBezierPolyline` 平移后的 spec + 控制多边形。
- [ ] 混合选择：盒+贝塞尔一次提交，`undo_steps` = 特征个数。
- [ ] 文案：不再写死 “Box and Sphere”；改为 `Copy currently supports Box, Sphere, and Bezier`（`.ts` 同步）。结果体（Boolean）仍走未实现的 DuplicateBody，保持失败提示。

### Bz5.3 AccuSnap

- [ ] ExtractEdges 已有折线后，现有边吸附应能打到近似折线；**不**单独做 Bezier 解析求交吸附（v1 足够）。

**Bz5 DoD：** 复制贝塞尔后两点距离 = 拾取偏移；undo 两步可还原。

---

## Bz6 — 选中控制网（可选，可延期）

**Files：** `PropertyPanel` 或 `SceneAdapter::mesh_for_body` 当 `SelectedTag` 且 type Bezier

- [ ] 选中时 `EdgeMesh` 追加 P0–P1–P2–P3 折线 + 十字（与预览同一 helper，抽到 `commands/tools/BezierPreview.h` 匿名命名空间或 `viewer` 小函数）。
- [ ] 未选中只画曲线（CVHIDE）。
- [ ] **不要**新 ISceneService 方法。

---

## 跨切任务（每个相关 PR 都要做）

| ID | 任务 |
|----|------|
| C1 | 4 空格 Allman、`m_`、DTO PascalCase；clang-format 仅改动文件 |
| C2 | Viewer 不 include `brep/bool/**`、`brep/build/**`（`MakeBezierWire` 仅 Part/Feature 用） |
| C3 | `ctest -R include_boundaries`（Bz3 加 Modeling.h 之后） |
| C4 | 日志 `BREP_INFO` 提交 Guid / 四点，便于调试 |
| C5 | 不在本功能修布尔数值、IoC、库拆分 |

---

## 文件清单（实现时对照）

| 区域 | 路径 |
|------|------|
| Kind / Spec | `Types.h`、`PrimitiveSpecs.h` |
| 曲线 | `Geometry.h/.cpp`、`Model.h/.cpp` |
| Wire 拓扑 | `Topology.h`、`Builder.cpp`、`PrimitiveBuild.h` |
| 校验 | `Validate.cpp` |
| 采样 / 显示 | `LoopSample.cpp`、`Mesh.cpp`、可选 `BezierSample.cpp` |
| 拷贝曲线 | `TopologyCopy.cpp` |
| 特征 | `BezierCurveFeature.h/.cpp`、`Part.*`、`FeatureHistory.*`、`BrepFeat.cmake` |
| IO | `XlDocument.cpp` |
| API | `api/Modeling.h`、`api/Mesh.h`（采样导出） |
| Adapter | `ISceneService.h`、`SceneAdapter.*`、`TestSceneAdapter.cpp` |
| 命令 | `CreateBezierTool.*`、`BuiltinCommands.cpp`、`CMakeLists.txt` viewer_runtime |
| UI | `MainWindowMenus.cpp`、`xcad_zh_CN.ts` |
| Copy | `CopyTool.cpp` |
| 测试 | `TestBezierCurve.cpp`、`TestBezierWire.cpp`、`TestBezierFeature.cpp` |

---

## 依赖与并行

```text
Bz1 ──────────────────────────────► Bz4（预览用 Eval/采样）
  └─► Bz2 ─► Bz3 ─► Bz4
                 └─► Bz5
Bz2.6 TopologyCopy ──（建议 Bz2 做完）──► 以后 DuplicateBody
Bz6 ── 仅依赖 Bz4
```

Bz1 与文档/菜单文案可并行；**Bz4 不得先于 Bz2.5**（否则提交后看不见线）。

---

## 验收清单（全部里程碑完成后）

- [ ] 建模菜单可启动命令；四点单击生成一条空间曲线。
- [ ] Backspace / ESC 行为符合规格 §4.1。
- [ ] 提交后线框可见；无三角面也不消失。
- [ ] 撤销/重做；存 `.xl` 再开曲线仍在。
- [ ] Copy：基点→目标点，副本四点平移。
- [ ] 中文 UI 下 prompt 为中文。
- [ ] 无 `BezierSpecFor`；`SpecFor` 返回 `BezierSpec`。
- [ ] `include_boundaries` 通过。

---

## 回滚

各 PR 只动上表路径。Bz1 回滚不影响 Viewer。Bz4 回滚：从 `register_builtin_commands` 和菜单去掉命令即可留下内核 Wire 能力。
