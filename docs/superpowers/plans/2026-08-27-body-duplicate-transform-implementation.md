# 实体复制 / 刚体变换 — 技术实现

> **治理：** [`docs/architecture/viewer-adapter-governance.md`](../../architecture/viewer-adapter-governance.md) §2、§8  
> **总排期：** [`2026-08-27-viewer-adapter-governance-implementation.md`](2026-08-27-viewer-adapter-governance-implementation.md)（本文 = **G1** + 体素 Move 细节）  
> **边界：** Viewer 只 `api/*`；拓扑观察指针见 `AGENTS.md`；布尔特解禁令 ADR 0008  
> **日期：** 2026-08-27

**Goal：** 让 Copy（以及随后的 Move）对 **任意实体** 成立，且 Adapter **不再按几何类型长虚函数**。内核提供 `DuplicateBody` / `TransformBody`；体素继续走 `SpecFor`，拉伸/融合等结果体走拓扑拷贝 + 刚体变换。

---

## 0. 现在满不满足开工条件？

**部分满足。可以开工的是刚体族第一阶段（平移 Copy / 体素 Move）。抽壳、曲线裁剪/延伸、镜像反射、参数化阵列现在不开工。**

### 已具备（本阶段依赖）

| 条件 | 现状 |
|------|------|
| 门面不再按类型 `XxxSpecFor` | 已有 `ISceneService::SpecFor` + `PrimitiveSpec` |
| 体素可重建 | `BoxFeature` / `SphereFeature` 的 `ToPrimitiveSpec` |
| 结果体拓扑深拷贝 | `CopyBodySubgraph`（`brep_bool`，Pipeline 已用） |
| 变换数学 | `RigidTransform` + `TransformPoint`（装配 Occurrence 在用） |
| 特征树 / 撤销 | `FeatureHistory` Append/Remove；`.xl` 按特征再生 |
| 命令骨架 | `ITool` + `CopyTool`（立方体/球） |
| Guid 身份 | ECS / 命令只持 `Guid`，不缓存 `Body*` |

### 本阶段缺口（实现时补，不阻塞立项）

| 缺口 | 处理 |
|------|------|
| 无 `Part::DuplicateBody` / `TransformBody` | P1 新增 |
| 几何无统一 `ApplyTransform` | P1：Point / Line / Circle / Plane / Sphere / Cylinder |
| 拷贝结果无法进特征树 | P1：`CopiedBodyFeature`（源 FeatureId + `T`） |
| `.xl` 不认识 CopiedBody | P1：schema 增一种特征类型 |
| `LineCurve` / `SphereSurface` 等缺少 Set* | P1 加变换所需突变接口 |

### 明确不满足（禁止夹带）

| 能力 | 原因 |
|------|------|
| 实体抽壳 Hollow | 无面等距；禁止再写 `BoxShell` 特解（ADR 0008）。见治理 §8.4 |
| 偏移 / 打断 / 裁剪 / 延伸 | 草图编辑 API 未立项；与抽壳不是一条路 |
| 镜像（含反射） | `det(T) < 0` 要处理面 `Sense` / 法向；平移 Copy 不需要 |
| 参数化阵列 `PatternFeature` | 第一阶段用 N 次 DuplicateBody 即可 |
| 拉伸/融合的 **就地 Move** | Regen 会丢掉裸拓扑平移；需 placement 挂在特征上（P3） |
| `ISceneService` 全量 PascalCase 改名 | 风格欠账，与本功能拆开 |
| NURBS 曲线/曲面变换 | 见下文「与 ADR 0009」：P1 遇 `Nurbs` Kind 失败；N1 落地后再给控制点仿射，**不提前做 NURBS 显示/求交** |

---

### 与 ADR 0009（NURBS）的关系 — 不改显示/建模范围

当前产品需求是解析体（盒/球/布尔/拉伸）的 Copy/Move，**不要求** 改 ADR 0009，也 **不立项** ADR 0011。

| 层 | 现在怎么做 | 何时才动 NURBS |
|----|------------|----------------|
| 显示 | 仍按 N1–N6：`NurbsCurve/Surface` + tessellate，mesh 不写回 B-Rep | 与 DuplicateBody **分 PR**；库已拆，tessellate 进 `brep_mesh` |
| `ApplyTransform` | 无载体：`CurveKind::Nurbs` / `SurfaceKind::Nurbs` → Duplicate **失败+诊断** | N1 有类之后：对 **控制点** 做 `TransformPoint`，权/节点不变（仿射）；不在本计划实现求交 |
| `PrimitiveSpec` / Adapter | **禁止** `NurbsCurveSpecFor` | 建模 UI 才出现 `Nurbs*Feature`（ADR 0011） |
| 抽壳等距 / NURBS 布尔 | 不做 | 解析面等距稳定 + 显示 N2+ 之后另 ADR |

盒子/球/当前布尔印记只有 Line/Circle/Plane/Sphere（及可能的柱面），P1 Duplicate **不会**碰到 NURBS。不要为 Copy 提前实现 `NurbsCurve`。

---

## Architecture

```text
CopyTool / 将来 MoveTool、ArrayTool
    │  Guid[] + 算出 RigidTransform（第一阶段只允许平移）
    ▼
ISceneService
    SpecFor(featureGuid, bodyGuid)           // 已有
    AddPrimitive(spec)                       // P2 收敛；P1 仍可 AddBox/AddSphere
    DuplicateBody(bodyGuid, T) → Body*       // P1：结果体
    TransformBody(bodyGuid, T) → bool        // P1：仅体素（改 spec）
    ▼
Part（brep_feat，可调用 brep_bool::CopyBodySubgraph）
    体素：变换 BoxSpec/SphereSpec → 原特征 Edit 或新 Append
    其它：CopiedBodyFeature(sourceId, T)::Rebuild
              CopyBodySubgraph(同一 Model)
              ApplyTransform(仅副本几何, T)
```

Viewer **不得** `#include "brep/bool/TopologyCopy.h"`。拷贝只通过 `Part` / `ISceneService`。

### 两条复制语义（必须写进实现，避免做错）

| | 体素 Copy | 结果体 Copy（拉伸/融合） |
|--|-----------|-------------------------|
| 做法 | 变换 spec，Append 新 Box/Sphere | `CopiedBodyFeature`：源特征 + `T` |
| `.xl` / Regen | 独立尺寸，改源不影响副本 | **派生**：Rebuild 时从 **当时的** 源体再拷一份 |
| 源被删 | 副本仍在 | Rebuild 失败（`Failed` + 诊断） |

独立「冻结 B-Rep 快照」需要把拓扑写入 `.xl`，**本阶段不做**。派生复制对融合体可接受：存档后打开 = 按当前源再生副本。

### 第一阶段 `T` 的约束

只允许 **平移**（`XAxis/YAxis/ZAxis` 为单位正交且右手，`Translation` 任意）。Copy/Move 拾取基点→目标点正好是平移。旋转/镜像放到后续里程碑。

---

## Global Constraints

- 新 API **PascalCase** 方法、camelCase 参数、DTO PascalCase；禁止新增 `snake_case` 虚函数。
- 不新增 `XxxSpecFor` / `AddCylinder` / `ShellBox`。
- `DuplicateBody` 入参 `Guid`，返回的 `Body*` 仅命令栈上使用，禁止存进 Tool 成员。
- `CopyBodySubgraph` 的 `Target` 必须是 **当前 Part 的 Model**（与 Pipeline 工作副本不同：Pipeline 拷进临时 Model）。
- 只变换 **副本** 上的 Point/Curve/Surface；源体几何禁止改。
- 每阶段可独立 PR；禁止夹带抽壳、NURBS 显示、IoC 新模块、全量 adapter 改名。
- 测试：内核 GoogleTest；Viewer `TestSceneAdapter`。本机 `cmake-build-mingw-debug`。

---

## 里程碑

| 阶段 | 交付 | 验收 |
|------|------|------|
| **P0** | 本文 + 治理互引 | 评审同意「派生 CopiedBody + 仅平移」 |
| **P1** | `ApplyTransform` + `DuplicateBody` + `CopiedBodyFeature` | 融合体/拉伸体 Copy 出独立显示体；undo；`.xl` 往返 |
| **P2** | CopyTool 统一；`TransformBody` 体素 Move | 球/立方体 Copy 仍走 spec；结果体走 Duplicate；Move 体素 |
| **P3** | （可选）Extrude/Boolean 就地 Move = 特征上叠加 T | 移动融合体后 Regen 位置保持 |
| **P4** | （可选）`AddPrimitive` 收敛 AddBox/AddSphere | `ISceneService` 创建侧不再按类型分叉 |
| **以后** | 旋转、镜像、阵列工具、Hollow、草图 Trim | 各需单独计划 |

---

## P0 — 文档（本文件）

- [x] 开工条件与非目标
- [x] 派生 vs 冻结语义
- [x] 治理文档 §7 链到本计划

---

## P1 — 内核：平移复制结果体

### P1.1 `RigidTransform` 平移判定与向量变换

**Files：** `kernel/include/brep/Plane.h`（或 `Math.h` 若更合适，保持 `RigidTransform` 现处）

- `TransformVector(v)`：忽略 Translation，用基底变换方向。
- `IsTranslation(eps)`：基底接近单位矩阵。
- 第一阶段 `DuplicateBody` 若 `!IsTranslation()` → 返回 nullptr + 日志（为 P3 旋转留口）。

### P1.2 几何 `ApplyTransform`

**Files：** `Geometry.h` / `Geometry.cpp`（`brep_core`）

自由函数（PascalCase）：

```cpp
void ApplyTransform(Point& p, const RigidTransform& t);
void ApplyTransform(Curve& c, const RigidTransform& t);
void ApplyTransform(Surface& s, const RigidTransform& t);
```

按 `Kind()` 分发：

| 类型 | 变换 |
|------|------|
| `Point` | `SetXyz(t.TransformPoint(Xyz()))` |
| `LineCurve` | origin 变点，direction 变向量并单位化 |
| `CircleCurve` | center 变点，normal/x/y 变向量 |
| `PlaneSurface` | origin 变点，u/v/normal 变向量并重建正交 |
| `SphereSurface` | center 变点，半径不变 |
| `CylinderSurface` | origin 变点，axis/x/y 变向量，半径不变 |
| 其它 Kind | 返回 false / 不变换，调用方失败 |

现有类若只有构造、没有 Set*：为变换补最小突变接口（`SetCenter` 等），**不要**改求值公式。

**测试：** `tests/kernel/TestApplyTransform.cpp`  
平移后球心、盒角点、平面 origin 符合 `T`；源对象未改（拷贝后再变）。

### P1.3 对拷贝子图施加变换

**Files：** `kernel/include/brep/bool/TopologyCopy.h`、`TopologyCopy.cpp`（`brep_bool`）

```cpp
void ApplyTransformToCopied(TopologyCopyContext& ctx,
                            const RigidTransform& t);
```

遍历 `ctx.Points` / `Curves` / `Surfaces` 的 **value**（副本），调用 P1.2。失败（未知 Kind）则让 `DuplicateBody` 整单失败，避免半变换体。

**测试：** 扩 `TestTopologyCopy`：拷贝盒子后平移，`ValidateBody` 仍 Ok；源盒 AABB 不变。

### P1.4 `CopiedBodyFeature`

**Files：** 新 `kernel/include/brep/feat/CopiedBodyFeature.h`、`kernel/src/feat/CopiedBodyFeature.cpp`；CMake `brep_feat`

- `TypeName()` → `"CopiedBody"`
- 成员：`m_sourceFeature`、`m_transform`、`m_bodyGuid`、`m_name`
- `ToPrimitiveSpec`：默认空
- `Rebuild`：
  1. 解析源特征 → `FindBody(source.BodyGuid())`，没有则失败
  2. 若已有 `m_bodyGuid` 对应体：先 `UnregisterBody` + 从 Model 摘掉旧副本（或 `RemoveBody` 同类路径，与 `RebuildSphereBody` 对齐，**禁止**裸 delete 拓扑）
  3. `TopologyCopyContext ctx{part.Model()}`；`CopyBodySubgraph(ctx, *src, "_copy")`
  4. `ApplyTransformToCopied(ctx, m_transform)`
  5. `RegisterBody`，`SetBodyGuid`

**测试：** `TestCopiedBody.cpp`  
融合两个盒子 → Duplicate 平移 → 两体共存；改源盒子尺寸 → Regen → 副本跟着变（派生语义）；删源 → 副本 Rebuild Failed。

### P1.5 `Part::DuplicateBody`

**Files：** `Part.h` / `Part.cpp`

```cpp
Body* DuplicateBody(brep::Guid sourceBodyGuid,
                    RigidTransform transform,
                    std::string name = "Copy");
```

- `FindBody`；`Features().FindByBody` 得源 `FeatureId`（找不到则失败：无特征的裸 Body 本阶段不支持）
- `CopiedBodyFeature::Create` → `AppendFeature` → `BodyForFeature`
- 内部 `IsTranslation` 检查

### P1.6 FeatureHistory + `.xl`

**Files：** `FeatureHistory.h/.cpp`、`XlDocument.cpp`、`XlDocument.h` 注释中的特征列表

- `FeatureTransaction` 增加 `CopiedSource` + `Transform`（或复用现有字段时 **必须在注释写清映射**，禁止 silently 复用 `Box` 存平移）
- `ApplyForward`：`FeatureType == "CopiedBody"` 时 Create + Append
- `.xl`：新 schema 或同 schema 增 type 字符串 `"CopiedBody"` + source Guid + 12 个 double（基底+平移）。旧文件无此类型仍能读。
- `api/Modeling.h` 已含 feat 头的话补 include CopiedBodyFeature（或仅经 Part 使用，Viewer 不直接 include 该头）

**测试：** 存盘再打开，CopiedBody 仍在且位置正确。

---

## P2 — Viewer：Copy 全对象 + 体素 Move

### P2.1 Adapter

**Files：** `ISceneService.h`、`SceneAdapter.*`、`TestSceneAdapter.cpp`

```cpp
Body* DuplicateBody(Guid bodyGuid, RigidTransform transform);
bool TransformBody(Guid bodyGuid, RigidTransform transform);
```

- `DuplicateBody` → `Part::DuplicateBody`
- `TransformBody`：`SpecFor` 有值则变换 spec 并 `set_box_params` / 等价 Edit（球：移 Center，半径不变）。无 spec → **P2 返回 false**（结果体 Move 归 P3）
- 禁止再加 `sphere_spec_for`

预览：Copy 结果体可用 `mesh_for_body` 变换顶点做线框，或简单 AABB 平移；不要在 Tool 里 include TopologyCopy。

### P2.2 CopyTool

**Files：** `CopyTool.cpp/.h`

收集源：

```text
spec = SpecFor(...)
  有值 → 列入 primitive 列表（现有 visit Box/Sphere）
  空   → 列入 bodyGuid 列表（DuplicateBody）
二者皆空且已选中 → 现有「仅支持…」改为「无法复制该对象」（仍可含无 Body 的草图）
```

提交：体素路径保持 `add_box`/`add_sphere`（P4 再收）；结果体 `DuplicateBody` + `record` 走 Part 历史（`undo_feature(n)` 已按步数撤销，CopiedBody 一次 Append = 一步）。

混合选择（一盒一融合）允许一次提交两种路径，`undo_steps` = 创建特征个数。

### P2.3 MoveTool（仅体素）— 已完成

**Files：** `MoveTool.cpp/.h`、`BuiltinCommands.cpp`、菜单/快捷键、`xcad_zh_CN.ts`  
**交互：** [move-command-interaction](../specs/2026-08-27-move-command-interaction.md)

- 复用 Copy 的基点→目标点 UI
- 提交 `TransformBody`；失败提示「移动目前支持立方体和球体」
- 命令 id：`edit.move`
- `TransformBody` 写入 `TxKind::TransformBody`，Undo 还原位置

---

## P3 — 结果体就地变换（可选，条件：P1 绿）

在 `CopiedBodyFeature` 之外，给 **已有** Extrude/Boolean 增加可选 `Placement`（默认恒等）。`Rebuild` 末尾对 **该特征产生的体** 做 `ApplyTransform`。

Move 融合体 = 改 Boolean 的 Placement，而不是 Duplicate+删源。

**不要**在 P1 做：会碰到「布尔工作体再变换」与 Pipeline 内部拷贝的纠缠，适合单独 PR。

---

## P4 — `AddPrimitive`（可选收敛）

```cpp
Body* AddPrimitive(const PrimitiveSpec& spec);
void RecordAppendPrimitive(feat::FeatureId id, PrimitiveSpec undo);
```

CopyTool / Create* 改走这一入口。`AddBox`/`AddSphere` 可留作薄封装。**本阶段不是 Copy 正确性的前置。**

---

## 以后（另开计划，本文不排期）

| 项 | 前置 |
|----|------|
| ArrayTool | P1 DuplicateBody；N 次平移 T |
| MirrorTool | 反射 `T` + 面 Sense；P1.2 支持 `det<0` |
| HollowFeature | 面等距内核；治理 §8.4 |
| 草图 Offset/Trim/Extend | `ISketchEdit` |
| 冻结拷贝 | `.xl` 存拓扑快照或 BKS 真源 |

---

## 建议实现顺序（单 PR 切片）

1. P1.1–P1.3 + `TestApplyTransform` / TopologyCopy 平移（无 Viewer）
2. P1.4–P1.6 + `TestCopiedBody` + xl 往返
3. P2.1–P2.2 Copy 融合体
4. P2.3 Move 体素

每片合并后：`ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure`；至少 `-R "CopiedBody|TopologyCopy|SceneAdapter|ApplyTransform"`。

---

## 回滚

各片只动列出的 feat/bool/Geometry/adapter/CopyTool。P1 失败时 Viewer 行为与现网一致（Copy 仍仅 Box/Sphere）。不要在失败片里改 Pipeline 求值顺序。
