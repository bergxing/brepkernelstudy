# Viewer Adapter 治理 — 执行计划

> **规范：** [`docs/architecture/viewer-adapter-governance.md`](../../architecture/viewer-adapter-governance.md)  
> **子计划：** [`2026-08-27-body-duplicate-transform-implementation.md`](2026-08-27-body-duplicate-transform-implementation.md)（G1 细节）  
> **日期：** 2026-08-27

**Goal：** 把治理文档从约定变成门面形状。`ISceneService` 只按职责长方法（身份、网格、生命周期、体素一个入口、刚体两个入口、运算特征至多一个入口），不再按几何类型长虚函数。用户能复制任意实体；创建/改参不再走 `AddBox`/`box_params` 分叉。

本文件是 **总排期**。G1 的内核切片、文件清单、测试名以 DuplicateBody 子计划为准，这里只写阶段边界与验收。

---

## 0. 现在满不满足开工条件？

**满足。G0 已落地，从 G1 开工。** 贝塞尔、抽壳、NURBS 显示、草图 Trim **不在本计划内**。

### 已完成（G0，对应治理 §3、§5 新代码）

| 项 | 现状 |
|----|------|
| 治理文档 v1.1 | `viewer-adapter-governance.md` |
| 类型分发在内核 | `IFeature::ToPrimitiveSpec`；`BoxFeature` / `SphereFeature` override |
| Adapter 一个读入口 | `ISceneService::SpecFor`（PascalCase） |
| Copy 体素 | `CopyTool` `visit PrimitiveSpec`（Box / Sphere） |
| 负例 | Boolean `SpecFor` 为空（`TestSceneAdapter`） |
| 禁止项 | 无 `XxxSpecFor`；圆柱/贝塞尔未加 `AddCylinder` / `BezierSpecFor` |

### 仍存在的错误模式（本计划要拆掉）

| 债务 | 位置 |
|------|------|
| `add_box` / `add_sphere` / `record_append_*` | `ISceneService` |
| `box_params` / `set_box_params` / `sphere_params` | 同上 |
| `SceneObject` 并行 `optional<BoxParams>` / `SphereParams` | `ISceneService.h` |
| `PropertyPanel::set_box_mode` / `set_sphere_mode` / `set_bezier_mode` | `PropertyPanel`（**G7** 拆除） |
| Copy 结果体失败 | `CopyTool` 只收集 `PrimitiveSpec`，无 `DuplicateBody` |
| 无刚体入口 | 缺 `DuplicateBody` / `TransformBody` |

### 明确不满足（禁止夹带）

与治理 §8.2 / §8.4、DuplicateBody 子计划「非目标」一致：

| 能力 | 原因 |
|------|------|
| Hollow / 面等距 | 无通用等距内核；禁止 `ShellBox`（ADR 0008） |
| 草图 Offset / Break / Trim / Extend | 需另立 `ISketchEdit` |
| 镜像（`det < 0`） | 面 Sense；平移 Copy 不需要 |
| 参数化 `PatternFeature` | G5 用 N 次 `DuplicateBody` 即可 |
| 特征图克隆 | 治理 §7 后期；不进 `PrimitiveSpec` |
| `ISceneService` 全量 snake_case → PascalCase | 风格批次，与本计划拆开（治理 §5） |
| NURBS 显示 / ADR 0011 | 不改 ADR 0009 |
| 贝塞尔命令 | 另计划；**G2 之后**才允许加 `BezierSpec` 臂，且禁止 `AddBezier` |

---

## Architecture（门面最终形状）

```text
ITool（Copy / Move / Array …）
    │  Guid + RigidTransform（工具算 T，不看几何种类）
    ▼
ISceneService
    SpecFor / AddPrimitive / SetPrimitive     // 体素唯一读写真入口
    DuplicateBody / TransformBody             // 刚体唯一入口
    AddBoolean                                // 运算（将来 AddShell 同类，本计划不加）
    ObjectFor* / MeshForBody / Undo*          // 身份与生命周期
    ▼
Part / Feature
    ToPrimitiveSpec                           // 能不能还原成 spec
    CopiedBodyFeature                         // 结果体派生复制
```

工具层 **看不到**「这是盒子还是融合体」的 Adapter 分发：`SpecFor` 有值走体素路径；否则走 `DuplicateBody`。禁止 `if (box) add_box; else if (sphere) add_sphere` 调两套虚函数。

---

## Global Constraints

- 新方法 **PascalCase**，参数 camelCase，DTO 字段 PascalCase。禁止新增 `snake_case` 虚函数。
- **`ISceneService` 虚表只允许在本计划标明的收敛点出现 diff**（G1 加两个刚体方法；G2 加 `AddPrimitive`；G4 加 `SetPrimitive` 并删 params 分叉）。加圆柱时虚表 **零 diff**（治理 §6）。
- Viewer 只 `#include "api/..."`。不得 include `brep/bool/TopologyCopy.h`。
- 命令 / ECS 只持 `Guid`；`Body*` 仅栈上。
- 每阶段可独立 PR。禁止夹带：抽壳、NURBS、IoC 新模块、全量 adapter 改名、贝塞尔 Tool。
- 测试：内核 GoogleTest；Viewer `TestSceneAdapter`。构建目录 `cmake-build-mingw-debug`。

```powershell
cmake --build cmake-build-mingw-debug
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure
```

---

## 里程碑

| 阶段 | 治理条款 | 交付 | 验收 |
|------|----------|------|------|
| **G0** | §3 | `SpecFor` + 体素 Copy | 已完成 |
| **G1** | §2.1、§7 DuplicateBody、§8.1 Copy | 任意实体平移复制 | 融合/拉伸 Copy 出新体；undo；`.xl` 往返；盒子/球仍走 spec |
| **G2** | §4、§7 AddPrimitive | 创建侧一个入口 | Copy / CreateBox / CreateSphere 走 `AddPrimitive`；虚表一次性收敛 |
| **G3** | §8.1 Move | `TransformBody` + MoveTool | **已完成** 体素可就地平移；结果体 Move 失败并提示（就地 Placement 见 G6） |
| **G4** | §7 SceneObject / `SetPrimitive` | 身份 DTO + 写入口 | **已完成**（无 `box_params`）；面板类型矩阵 **未** 拆净 |
| **G5** | §8.1 阵列 | ArrayTool = N× DuplicateBody | 不引入 `PatternFeature` |
| **G6** | §8.1 结果体 Move | Extrude/Boolean `Placement` | 可选；条件 G1 绿 |
| **G7** | §9 PropertySheet | 面板与形体解耦 | 无 `set_*_mode`；加圆柱不改 `PropertyPanel.h` |
| **以后** | §7 特征克隆、§8.2–§8.4 | 另开计划 | 见文末停放表 |

**推荐 PR 切片：** G1 内核 → G1 Viewer Copy → G2 → G3 → G4。G5 / G6 不阻塞 G2–G4。

**产品优先序：** G1 解决「所有对象都能复制」。G2 堵住下一周 `AddCylinder`。G3 是同一套拾取的 Move。G4 瘦身份 DTO。G7 拆属性面板类型矩阵。不要先做 G7 再做 G1。

---

## G0 — 已完成（对照清单）

- [x] `PrimitiveSpec` = `variant<BoxSpec, SphereSpec>`
- [x] `ISceneService::SpecFor`
- [x] CopyTool 体素 `visit`；中文提示
- [x] `TestSceneAdapter`：`SpecForBox` / `SpecForSphere` / Boolean 为空
- [x] 治理文档；`layering.md` / `ioc-governance.md` 互引

---

## G1 — 结果体平移复制（先做）

**子计划全文：** [body-duplicate-transform](2026-08-27-body-duplicate-transform-implementation.md) P1 + P2.1–P2.2。

Adapter 只加（PascalCase）：

```cpp
Body* DuplicateBody(Guid bodyGuid, RigidTransform transform);
bool TransformBody(Guid bodyGuid, RigidTransform transform);  // G3 才给 Move 用；G1 可先声明、体素实现
```

CopyTool：`SpecFor` 有值 → 体素列表；空且有 Body → `DuplicateBody`。混合选择一次提交两条路径。

**本阶段仍允许** Copy 体素走 `add_box` / `add_sphere`（G2 再收）。`TransformBody` 结果体返回 false。

**不要做：** 冻结 B-Rep 快照、旋转、镜像、结果体就地 Move。

**验收命令（最少）：**

```powershell
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure -R "CopiedBody|TopologyCopy|ApplyTransform|SceneAdapter"
```

手工：融合两盒 → Copy 平移 → 两体共存；Undo 去掉副本。

---

## G2 — `AddPrimitive` 收敛

治理 §4：创建侧将来收成一个入口。这是 **虚表允许出现的一次性 diff**。

### G2.1 接口

**Files：** `ISceneService.h`、`SceneAdapter.*`、`TestSceneAdapter.cpp`

```cpp
Body* AddPrimitive(const PrimitiveSpec& spec);
void RecordAppendPrimitive(feat::FeatureId id, PrimitiveSpec undo);
```

内部 `visit`：`BoxSpec` → 现有 `Part::AddBox`；`SphereSpec` → `AddSphere`。  
`add_box` / `add_sphere` / `record_append_feature` / `record_append_sphere`：**本阶段改为非虚薄封装或删除**。优先删除虚函数，调用点全部改走 `AddPrimitive`，避免门面继续摆两套。

若测试/外部暂时需要盒子快捷方式，放在 `SceneAdapter` 的 **非虚** 辅助，不进 `ISceneService`。

### G2.2 命令

**Files：** `CreateBoxTool.cpp`、`CreateSphereTool.cpp`、`CopyTool.cpp`

- 提交只调 `AddPrimitive` + `RecordAppendPrimitive`。
- Copy 体素：变换后的 spec 仍 `visit` 做预览线框，提交不再 `add_box`/`add_sphere`。

### G2.3 内核（若尚未有）

`Part` 已有 `AddBox`/`AddSphere` 即可，不必先做 `Part::AddPrimitive`。分发留在 Adapter 这一层也可以；若 `Part` 加 `AddPrimitive`，Viewer 更薄。二选一，**不要两处都 visit 且语义不一致**。倾向：`Part::AddPrimitive` 一次，Adapter 转发。

**验收：** `ISceneService.h` 不再出现 `add_box` / `add_sphere`。`TestSceneAdapter` 创建盒子/球走 `AddPrimitive`。虚表除 G2 所列外无新 `AddXxx`。

**闸门：** G2 合并前，禁止把圆柱/贝塞尔做成 `AddCylinder`/`AddBezier`。G2 之后新体素只加 variant 臂（治理 §6）。

---

## G3 — `TransformBody` + 体素 Move（已完成）

对应 DuplicateBody 子计划 P2.3。交互流程：[2026-08-27-move-command-interaction.md](../specs/2026-08-27-move-command-interaction.md)。

- [x] `Part::TransformBody` 写入 `TxKind::TransformBody`（可 undo）
- [x] `MoveTool` / `edit.move` / 菜单 `Ctrl+Shift+M` / i18n
- [x] 融合体 `TransformBody` 返回 false；盒子 undo 还原位置

### G3.1 Adapter

```cpp
bool TransformBody(Guid bodyGuid, RigidTransform transform);
```

- `SpecFor` 有值：只允许 **平移**（与 G1 `IsTranslation` 相同）；改 spec 后写回特征（G4 前可用现有 `set_*_params` + 原点/球心字段；G4 后只走 `SetPrimitive`）。
- 无 spec：返回 false（拉伸/融合就地 Move → G6）。

### G3.2 MoveTool

**Files：** 新 `MoveTool.cpp/.h`、`BuiltinCommands.cpp`、菜单、`xcad_zh_CN.ts`

- 命令 id：`edit.move`
- UI 与 Copy 相同：基点 → 目标点；**不**新增长 Adapter 方法。
- 失败提示：目前仅支持可还原体素（立方体/球体）。

**验收：** 移动盒子后尺寸不变、角点平移；球心平移、半径不变；融合体 Move 失败不损坏模型。

---

## G4 — `SceneObject` 瘦身 + 属性面板 registry

治理 §7 两行：身份 DTO 不再塞尺寸；面板不按类型调 Adapter。

### G4.1 写入口（与 `SpecFor` 配对）

```cpp
bool SetPrimitive(feat::FeatureId id, const PrimitiveSpec& spec);
```

- Box 臂：改 LWH / 位置（与现 `set_box_params` 等价，可顺带吃进原点若 spec 已有）。
- Sphere 臂：改半径 / 球心。
- 错误臂或特征类型不匹配：false。

删除虚函数：`box_params`、`set_box_params`、`sphere_params`、`set_sphere_params`。读尺寸一律 `SpecFor`。

`boolean_params` 可暂时保留（运算特征，不是体素）。不要为此加 `BooleanSpecFor`。长期与 `AddBoolean` 一起考虑 `AddFeature`，**本阶段不动布尔创建**。

### G4.2 `SceneObject`

只留：

```cpp
struct SceneObject
{
    Guid BodyGuid;
    Guid FeatureGuid;
    std::string Name;
    std::string TypeName;
};
```

字段改为 PascalCase（DTO 规范）。同步改 ECS/属性面板所有读写。`optional<BoxParams>` / `SphereParams` / 若只为面板服务的 `BooleanParams` 从 DTO 移除；布尔属性若还要显示 op，用 `TypeName == "Boolean"` + 现有 `boolean_params` 或单独查询，**不要**再往 DTO 加 `optional<CylinderParams>`。

G4 是 `SceneObject` 字段改名窗口。本仓库已把 `ISceneService` 存量方法一并改成 PascalCase（`SetDocument` / `MainPart` / …）。

### G4.3 属性面板（已过时，改走 G7）

G4.3 原计划「Panel 内 visit + TypeName 表」。落地后仍留下 `set_box_mode` / `set_sphere_mode`，Bezier 又加了 `set_bezier_mode`。

**废止该妥协。** 对象/UI 解耦见治理 §9、规格 [property-sheet-design](../specs/2026-09-16-property-sheet-design.md)、计划 [property-sheet-implementation](2026-09-16-property-sheet-implementation.md)（**G7**）。

G4.1 `SetPrimitive`、G4.2 瘦 `SceneObject` **已完成**，不必重做。

---

## G7 — PropertySheet（属性面板）

全文：[2026-09-16-property-sheet-implementation.md](2026-09-16-property-sheet-implementation.md)。治理 §9；规格 §5.1。

本总计划不重复切片。G7 **不**改 `ISceneService` 虚表。

面板 private：**删** `set_*_mode`、`on_dim_edited`、`on_weight_edited`、`refresh_dim_hint`、`block_*_signals`；**留** `set_enabled`。写回只留 `OnFieldEdited(fieldId)`。

---

## G5 — 阵列（可选，薄）

`ArrayTool`：算出 `T_0 … T_{N-1}`（第一阶段仅平移网格），对每个 `T_i` 调 `DuplicateBody`（体素则 `AddPrimitive` 变换后的 spec）。

- 不向 Adapter 加 `ArrayBody`。
- 不做 `PatternFeature`（改个数再生）。
- 条件：G1 绿。可与 G3 并行，但不要和 G1 挤同一个 PR。

---

## G6 — 结果体就地 Move（可选）

DuplicateBody 子计划 P3：Extrude/Boolean 增加 `Placement`，`Rebuild` 末尾对 **该特征产生的体** `ApplyTransform`。

Move 融合体 = 改 Placement，不是 Duplicate+删源。  
**不要**并进 G1。

---

## 停放（另开计划，本文不排期）

| 项 | 治理 | 前置 |
|----|------|------|
| 特征图克隆（连草图/操作数复制） | §2.1、§7 | G1 派生语义先用着 |
| 冻结拷贝（`.xl` 存拓扑） | §2.1 | schema |
| MirrorTool | §8.1 | `ApplyTransform` 支持 `det<0` + Sense |
| `ISketchEdit`（偏移/打断/裁剪/延伸） | §8.2 | 草图实体图 API |
| `AddShell` / Hollow | §8.4 | 面等距 + Pipeline；禁止 `ShellBox` |
| 贝塞尔曲线命令 | §6 清单 | **G2 完成**；`BezierSpec` 进 variant；`WireEdges`；禁止 `BezierSpecFor` |
| NURBS 显示 N1–N6 | ADR 0009 | 与本计划分 PR |
| adapter 存量 `set_document` 等改名 | §5 | style-migration 批次 |

---

## 阶段依赖

```text
G0 已完成
 └─ G1 DuplicateBody + Copy 全对象     ← 当前开工点
      ├─ G2 AddPrimitive               ← 堵住 AddXxx
      │    └─ （之后才允许新体素 / 贝塞尔进 variant）
      ├─ G3 TransformBody + Move 体素     ← 已完成
      ├─ G5 Array（N 次 Duplicate）
      └─ G6 结果体 Placement Move
 G2 + G3 之后 → G4 SceneObject / SetPrimitive     ← 已完成
 G4 之后 → G7 PropertySheet（面板）
           见 [2026-09-16-property-sheet-implementation.md](2026-09-16-property-sheet-implementation.md)
```

G3 与 G2 无硬依赖。G7 依赖 G4 的 `SetPrimitive`，不改 Adapter 虚表。

---

## 回滚

| 阶段 | 范围 |
|------|------|
| G1 | Geometry `ApplyTransform`、TopologyCopy 变换、`CopiedBodyFeature`、Part、xl、CopyTool、Adapter 两方法 |
| G2 | `AddPrimitive` + 三处 Tool；可恢复薄 `add_box` |
| G3 | MoveTool + `TransformBody` 体素分支 |
| G4 | `SceneObject`、`SetPrimitive`、params 删除 |
| G7 | `PropertySheet.*`、`PropertyPanel` 动态行（见 G7 子计划） |

失败时 Viewer Copy 不得劣于 G0（盒子/球仍可复制）。不要在失败片里改 Boolean Pipeline 求值顺序。
