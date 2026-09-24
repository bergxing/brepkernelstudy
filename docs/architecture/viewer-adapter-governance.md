# Viewer Adapter 治理：禁止按几何类型长接口

**版本**：1.2  
**日期**：2026-09-16  
**关联**：[layering.md](layering.md) §4.7、[ioc-governance.md](ioc-governance.md)、[style-migration-plan.md](style-migration-plan.md)  
**属性面板：** [property-sheet-design](../superpowers/specs/2026-09-16-property-sheet-design.md)（G7）

`ISceneService` 是 Viewer 对 Document/Part 的门面（Guid ↔ 特征，**不**长期持有 `Body*`）。它不是 CAD 内核的「每一种几何一个虚函数」目录。

---

## 1. 问题

错误模式：

```cpp
virtual std::optional<BoxSpec> BoxSpecFor(...);
virtual std::optional<SphereSpec> SphereSpecFor(...);
virtual std::optional<CylinderSpec> CylinderSpecFor(...);  // 下一周
virtual std::optional<NurbsCurveSpec> NurbsCurveSpecFor(...);  // 爆炸
```

每加一种几何，Adapter 接口、SceneAdapter、CopyTool、属性面板、测试同步加一组方法。这违反接口隔离（I.*）：门面在记录类型目录，而不是身份与生命周期。

属性面板上的 `set_box_mode` / `set_sphere_mode` / `set_bezier_mode` 是 **同一错误在 Widget 上的翻版**（G7 拆除，见 §9）。

同类债务已经存在：`add_box` / `add_sphere`、`box_params` / `sphere_params`、`SceneObject` 上的 `optional<BoxParams>` / `optional<SphereParams>`。**新代码不得再复制这套模式。**

---

## 2. 对象分三类，复制策略不同

不是所有「能画出来的东西」都该进 `SpecFor`。

| 类别 | 例子 | 如何复制 | 是否进 `PrimitiveSpec` |
|------|------|----------|------------------------|
| **参数体素** | Box、Sphere、未来 Cylinder | 读 spec → 平移 → `AddPrimitive` | **是** |
| **结果实体** | Extrude、Loft、Boolean（融合/切割/交集） | **体副本 + 刚体变换**（见 §2.1） | **否** |
| **草图 / 曲线** | 直线、NURBS 曲线、圆弧 | 草图变换 API；显示 ADR 0009，建模 ADR 0011 | **否** |

圆柱可以变成 `CylinderSpec` 并加入 variant。直线和 NURBS 曲线 **不要** 做成 `ISceneService::LineSpecFor`。

### 2.1 拉伸 / 放样 / 融合：不要做成第四、第五、第六个 Spec

这三类都是 **输入 → 运算 → 结果体**，不是「一组可重建的解析尺寸」：

| 特征 | 现状 | 输入 | 结果 |
|------|------|------|------|
| 拉伸 `Extrude` | 已有 `ExtrudeFeature` | 草图 + 距离 | 一个 `Body` |
| 放样 Loft | 尚未立项 | 多截面（+ 可选轨线） | 一个 `Body` |
| 融合 Fuse | 已有 `BooleanFeature`（Union） | 两个特征 + `BooleanOp` | 一个 `Body` |

用户选中结果体再 Copy，要的是 **另一份实体在新位置**，不是再执行一次拉伸/放样/布尔。

因此 Adapter **只增加一种与类型无关的能力**（落地时 PascalCase）：

```text
DuplicateBody(bodyGuid, offset) → 新 Body*
```

内核路径已有半成品：`CopyBodySubgraph`（拓扑深拷贝）+ 对拷贝后的点/几何做刚体平移。新体挂一个独立特征（例如 `CopiedBody`），**不**引用源拉伸/放样/布尔的 FeatureId。这样：

- CopyTool **一条**分支：`SpecFor` 有值走体素；否则走 `DuplicateBody`
- 加放样时：`LoftFeature::ToPrimitiveSpec` 保持默认空；`ISceneService` **零 diff**
- 融合体与拉伸体走同一条 `DuplicateBody`，不必 `FuseSpecFor` / `ExtrudeSpecFor`

**不要**把 Extrude 的草图+距离、Loft 的截面、Boolean 的 target/tool 塞进 `PrimitiveSpec`。那会把 variant 变成特征树序列化，复制语义也会错（融合体会把两个操作数再布尔一遍）。

参数化「连历史一起复制」（克隆草图再拉伸、连操作数一起融合）是 **第二阶段**，走特征图克隆，与 `DuplicateBody` 分开；第一阶段先保证所有结果体都能搬位置。

---

## 3. 类型分发放在内核，不放在 Adapter 虚表

```text
IFeature::ToPrimitiveSpec(params)     ← 各特征自己知道能不能还原成 spec
        ↓
ISceneService::SpecFor(featureGuid, bodyGuid)   ← 只做 Guid 解析，接口不再涨
        ↓
ApplyTransform(PrimitiveSpec, T)      ← 内核：平移 spec（加圆柱只加 variant 臂）
Copy / Move                           ← 只持 Guid + T；预览平移 mesh_for_body 边
```

- 新增体素：扩展 `brep::PrimitiveSpec`（`std::variant<BoxSpec, SphereSpec, …>`），在对应 `*Feature` 上 override `ToPrimitiveSpec`，并给 `ApplyTransform(PrimitiveSpec)` 加一臂。
- **`ISceneService` 不增加方法。**
- Boolean / Extrude 保持基类默认 `nullopt`。

Copy 体素：`SpecFor` → `ApplyTransform` → `AddPrimitive`（副本仍是独立 Box/Sphere 特征）。Copy 结果体：`DuplicateBody`。Move：`TransformBody`。命令里 **不要** `TranslatedBox` / `MakeSphereWire` 这类按类型特写。

禁止再写 `if (box) SpecForBox; else if (sphere) SpecForSphere` 去调两套 Adapter API。

---

## 4. `ISceneService` 允许长什么、禁止长什么

**允许（按职责，不是按几何）：**

- 身份：`ObjectForBody` / `ObjectForFeature` / `FeatureIdFor`
- 网格：`MeshForBody`
- 生命周期：`RemoveFeature`、`UndoFeature` / `RedoFeature`
- 体素读/写的 **一个** 入口：`SpecFor`；创建侧将来收成 `AddPrimitive(const PrimitiveSpec&)`
- 布尔是 **运算**，不是体素：可保留 `AddBoolean`（两特征 + op），不要为 Union/Subtract 各写一套 SpecFor

**禁止：**

- `XxxSpecFor` / `AddXxx` / `SetXxxParams` / `RecordAppendXxx` 按几何类型无限追加
- 在 Adapter 里 `static_cast` 到 `BoxFeature*` / `SphereFeature*` 再暴露给命令（命令只见 `api/*` 与 spec）
- 为草图曲线、NURBS 显示网格单独在 `ISceneService` 上开平行虚函数；曲线走草图/显示管线

现有 `AddBox` / `AddSphere` / `box_params` 视为 **过渡便利函数**。加圆柱时走 `PrimitiveSpec` + `ToPrimitiveSpec`，不要加 `AddCylinder` 到接口；随后再把 Box/Sphere 创建收到 `AddPrimitive`。

---

## 5. 命名（新代码强制）

与 [google-cpp-style/project-overrides.md](../../.cursor/skills/google-cpp-style/project-overrides.md) 一致：

| 元素 | 风格 | 本门面示例 |
|------|------|------------|
| 接口方法 | **PascalCase** | `SpecFor`、`ObjectForBody` |
| 参数 / 局部变量 | camelCase | `featureGuid`、`bodyGuid` |
| struct / DTO 字段 | PascalCase | `BoxSpec::Min`、`SceneObject` 新字段 |
| 类成员 | `m_` + camelCase | `SceneAdapter::m_document` |

`ISceneService` 上存量方法已在 G4 收成 PascalCase（`SetDocument`、`MainPart`、`MeshForBody` 等）。不要再新增 `snake_case` 方法。

---

## 6. 加一种体素的检查清单（Cylinder 为例）

1. `PrimitiveSpecs.h`：`CylinderSpec` 加入 `PrimitiveSpec` variant  
2. `CylinderFeature::ToPrimitiveSpec`  
3. `Part` 创建/重建（内核）  
4. CopyTool：`visit` 增加平移与线框臂（或等 `AddPrimitive` 落地后只改 Adapter）  
5. 属性面板：**只**给 `Describe(CylinderSpec)` / `Apply` 加臂（§9），**不**改 `PropertyPanel.h`，**不**给 `ISceneService` 加 `cylinder_params`  
6. 测试：`SpecFor` 命中 `CylinderSpec`；Boolean 仍为空  

接口文件 `ISceneService.h` 的虚函数表 **不应出现 diff**（除将来一次性的 `AddPrimitive` 收敛）。`PropertyPanel.h` 加圆柱时同样 **不应出现 diff**。

---

## 7. 后续收敛（未在本变更完成）

执行排期（G0 已完成；G4.1/G4.2 已落地；属性面板见 G7）：[2026-08-27-viewer-adapter-governance-implementation.md](../superpowers/plans/2026-08-27-viewer-adapter-governance-implementation.md)。G1 内核细节：[body-duplicate-transform](../superpowers/plans/2026-08-27-body-duplicate-transform-implementation.md)。G7：[property-sheet-implementation](../superpowers/plans/2026-09-16-property-sheet-implementation.md)。

| 项 | 阶段 | 说明 |
|----|------|------|
| `DuplicateBody(bodyGuid, T)` | G1 | 平移复制结果体 |
| `AddPrimitive` + `RecordAppendPrimitive` | G2 | Copy / 创建工具不再调用 `AddBox`/`AddSphere` |
| `TransformBody` + Move | G3 | 体素就地平移；交互见 [move-command-interaction](../superpowers/specs/2026-08-27-move-command-interaction.md)；结果体 Move 见 G6 |
| `SceneObject` 去掉并行 `optional<BoxParams>` | G4 | 身份 DTO 只留 Guid / TypeName / Name；`SetPrimitive` 已落地 |
| 属性面板 `set_*_mode` | **G7** | `PropertySheet` + `Describe`/`Apply`，见 [property-sheet-design](../superpowers/specs/2026-09-16-property-sheet-design.md) |
| 阵列 N× DuplicateBody | G5 | 不引入 `PatternFeature` |
| 特征图克隆 | 停放 | 连草图/操作数一起复制；不进 `PrimitiveSpec` |

---

## 8. 修改类命令：Copy 之后的 Move / 阵列 / 镜像 / 偏移 / 打断 / 裁剪 / 延伸

命令可以很多，**内核能力只有两摊**。禁止 `MoveBox` / `ArraySphere` / `TrimExtrude` 这种命令×类型矩阵。

### 8.1 刚体族（Copy、Move、阵列、镜像）

这些命令的差异只在 **生成哪些 `RigidTransform`**，以及 **是否留下源体**：

| 命令 | 是否保留源 | 变换个数 | UI 负责算什么 |
|------|------------|----------|----------------|
| Copy | 是 | 1 | 基点 → 目标点 → `T` |
| Move | 否 | 1 | 同一套拾取 → `T` |
| 镜像 | 可选（复制或就地） | 1 | 镜面 → 反射 `T`（`det = -1`） |
| 阵列 | 是 | N | 行/列/角度 → `T_0 … T_{N-1}` |

内核 / Adapter **只保留两个入口**（Guid，不持有 `Body*`）：

```text
TransformBody(bodyGuid, T)      // 就地：Move、就地镜像
DuplicateBody(bodyGuid, T)      // 副本：Copy、阵列每一份、复制镜像
```

体素（Box/Sphere）Copy 走 `SpecFor` + `ApplyTransform(PrimitiveSpec)` + `AddPrimitive`（副本仍是独立体素特征）；拉伸/融合走 `DuplicateBody`。**工具层按「能不能还原成 spec」分支，不按盒子/球分支。** 加圆柱只改 variant 与 `ApplyTransform` 一臂。

阵列第一阶段做成 **N 次 DuplicateBody**（独立体）。参数化 `PatternFeature`（改个数能再生）是第二阶段，与特征图克隆同类，不阻塞第一阶段。

共享拾取：Copy 已有的「基点 → 目标点」就是 Move 的 UI；阵列/镜像是同一 `ITool` 模式、不同 prompt，**不要**各写一套 SceneAdapter 方法。

仓库里已有 `RigidTransform`（装配 occurrence 在用）。实体修改应作用在 **Part 内拓扑/特征**，不要把实体移动做成装配级 Occurrence——那是另一套问题。

### 8.2 曲线族（偏移、打断、裁剪、延伸）

这些作用在 **边 / 草图曲线 / 线框**，不是闭实体：

| 命令 | 操作对象 | 内核落点 |
|------|----------|----------|
| 偏移 Offset | 草图曲线或面轮廓 | 草图 offset；实体抽壳/等距是另一条（Face offset），勿混名 |
| 打断 Break | 曲线上一点 → 两条 | 草图 split vertex |
| 裁剪 Trim | 曲线被另一曲线切断 | 草图 trim |
| 延伸 Extend | 曲线接到边界 | 草图 extend |

**不要**挂在 `ISceneService` 的实体门面上。单独 `ISketchEdit`（或草图命令直接调 `api/Modeling.h` 里的草图 API）。实体上的「裁剪」若指切掉一块，那是 **Boolean Subtract**，已有 `AddBoolean`，不是 Trim。

NURBS 曲线的 trim/extend 等 ADR 0011 建模立项后再做；显示（ADR 0009）不包含这些编辑。

### 8.3 分层

```text
ITool（Copy / Move / BooleanTwoBodyTool / Array / Mirror / Shell / Trim …）
    │  只收集 Guid + 意图（T、壁厚、开口面、曲线编辑）
    ▼
ISceneService          刚体：TransformBody / DuplicateBody
                       造型运算：AddBoolean、将来 AddShell（与 AddBoolean 同类）
ISketchEdit（未来）    曲线：Offset / Split / Trim / Extend
    ▼
kernel                 CopyBodySubgraph + RigidTransform
                       BooleanPipeline
                       ShellFeature（面等距 + 内壳，见 §8.4）
                       Sketch 实体图编辑
```

加 Copy/Move/阵列 = 加 `ITool`，Adapter 刚体入口不变。  
并 / 减 / 交共用一个 `BooleanTwoBodyTool` + `AddBoolean`，不要为 Union/Intersect 再写 Instant「必须先选两体」命令；交互见 [2026-09-17 两体交互](../superpowers/specs/2026-09-17-boolean-two-body-interaction.md)。  
抽壳这类 **造型运算** 与布尔同类：内核一个 `*Feature`，Adapter **至多一个** `AddShell`，禁止 `ShellBox` / `ShellSphere`。

### 8.4 实体抽壳（Solid Shell / Hollow）

拓扑里已有 `class Shell`（面集合），**抽壳命令不要再用这个词当类型名**，以免和 `Body::Shells` 混淆。特征用 `ShellFeature` / TypeName `"Hollow"`。

抽壳是 **第三类：造型运算特征**，和 Boolean / Extrude 一档，不是刚体，也不是草图偏移。

| | 抽壳 | 不要做成 |
|--|------|----------|
| 输入 | 一个目标特征 + 壁厚 + 可选开口面 Guid | `ShellBoxSpec` / `PrimitiveSpec` 新臂 |
| 输出 | 新 `Body`（通常多一个内壳，或开口后变成薄壁） | `TransformBody` |
| 再生 | 改厚度 / 改开口面 → `Rebuild` | 对源 Box 改 LWH 冒充抽壳 |
| 复制 | 结果体走 `DuplicateBody`；`ToPrimitiveSpec` 为空 | `ShellSpecFor` |

语义（常见 CAD）：选实体 → 选要去掉的面（开口，可空）→ 输入厚度（向内/外）→ 其余面等距，封出内腔或薄壁件。

**实现约束（对齐 ADR 0008）**：禁止 `BoxShell.cpp` / `SphereShell.cpp` 体素特解。通用路径是：

1. 对每个保留面做 **等距曲面**（平面=平移，球面=改半径，一般 NURBS 等距是后期）；
2. 开口面不参与，其邻面延伸/裁剪到等距面；
3. 用现有 **Imprint + Boolean** 把内壳缝进实体（闭抽壳 ≈ 原体外壳 ∪ 内壳；开口抽壳 ≈ 薄壁实体）。

体素上的「盒子挖空、球变厚球壳」只是通用路径在平面/球面上的特例，由 `SurfaceKind` 等距实现，**不**单独注册体对体入口。

**分阶段：**

| 阶段 | 范围 | 说明 |
|------|------|------|
| 0 文档 | 本小节 | 接口形状冻结：`ShellFeature(target, thickness, openFaceGuids)` |
| 1 解析面 | 全平面体、球面 | 平面法向平移、球半径 ±t；开口面先只支持整面删除 |
| 2 通用 | 布尔结果、拉伸体 | 依赖面等距 + Pipeline；失败则 `FeatureStatus::Failed` + 诊断 |
| 3 相关运算 | 加厚 Thicken、单面 OffsetFace | **同一套面等距内核**，不同 Feature 包装；不要再开第四条 Adapter API |

Viewer：`HollowTool` 拾取体 / 面 / 厚度；Adapter 若暂时跟 `AddBoolean` 对称，只加 **一个** `AddShell(...)`。长期与 Extrude/Boolean/Hollow 一起收进 `AddFeature(ModelingOp)`，避免运算特征再按名字长虚函数。

失败模式要可诊断（壁厚大于局部曲率半径、自交、开口后面不流形），走特征 `Failed` + 消息，不要静默出破面。

---

## 9. 属性面板：对象与 UI 解耦（G7）

细则：[property-sheet-design](../superpowers/specs/2026-09-16-property-sheet-design.md)；步骤：[property-sheet-implementation](../superpowers/plans/2026-09-16-property-sheet-implementation.md)。

G4 收掉了 Adapter 上的 `box_params`，但 `PropertyPanel::set_box_mode` / `set_sphere_mode` / `set_bezier_mode` 把类型目录搬进了 Widget。这与 §1 禁止的长接口同类，**不得再增加** `set_cylinder_mode`。

**允许：**

```text
PrimitiveSpec  --Describe/Apply（内核，无 Qt）-->  PropertySheet
PropertyPanel  只按 PropertyKind 画 Spin / SliderSpin / 只读
写回：Apply + 已有 SetPrimitive
```

**禁止：**

- 面板头文件出现 `BoxSpec` / `set_*_mode` / `on_dim_edited` / `on_weight_edited` / `refresh_dim_hint` 按形体分支
- `ISceneService::PropertySheetFor` / `SetBoxParams`（门面不按几何涨）
- 内核 include Qt；IoC 插件式 `IPropertyEditor`

写回只允许 `OnFieldEdited(fieldId)` → `Apply` → `SetPrimitive`。`set_enabled`（有无选中）不是类型特化，允许保留。

加圆柱：治理 §6 清单 1–4 + `Describe`/`Apply` 一臂。`ISceneService.h` 与 `PropertyPanel.h` 均零 diff。

G4.3 曾允许「Panel 内 visit 当第一阶段 registry」。已废止。字段投影只允许出现在 `Describe`/`Apply`（与 `ApplyTransform` 同层）。

结果实体（Boolean / Extrude）：`SpecFor` 为空 → 身份 + 只读 Hint，不要为运算特征做 `XxxSpec` 只为了出面板。

长期（不阻塞 G7）：面板数据源收到 `ParameterStore`（盒子/球尺寸已在库中）。G7 仍走 Spec 投影，避免与再生双写。
