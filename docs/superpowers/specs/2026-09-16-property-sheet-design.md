# 属性面板 — 对象与 UI 解耦

**日期：** 2026-09-16  
**状态：** 设计（G7，接 G4）  
**治理：** [viewer-adapter-governance.md](../../architecture/viewer-adapter-governance.md) §9  
**实施：** [2026-09-16-property-sheet-implementation.md](../plans/2026-09-16-property-sheet-implementation.md)  
**关联：** [Adapter 治理执行计划](../plans/2026-08-27-viewer-adapter-governance-implementation.md) G4、[`PropertyPanel`](../../../apps/viewer/PropertyPanel.h)、[`PrimitiveSpecs.h`](../../../kernel/include/brep/feat/PrimitiveSpecs.h)、[`ParameterStore`](../../../kernel/include/brep/param/Parameter.h)

**Goal：** 属性面板不再按形体长 `set_box_mode` / `set_sphere_mode` / `set_bezier_mode`。对象给出一张无 Qt 的字段表；Widget 只按 `Kind` 画控件。加圆柱时 **PropertyPanel 头文件零 diff**。

---

## 1. 问题

治理已经禁止 `ISceneService` 按几何类型长虚函数（`XxxSpecFor`）。G4 收掉了 `box_params` / `SceneObject` 上的尺寸 optional，读写真入口变成 `SpecFor` / `SetPrimitive`。

债务挪到了 **Qt 面板**：

```cpp
void set_box_mode(bool on);
void set_sphere_mode(bool on);
void set_bezier_mode(bool on, int cvCount);
```

`ShowEntity` 里 `holds_alternative<BoxSpec>` / `SphereSpec` / `BezierSpec`，编辑再 `visit` 写回。贝塞尔权重又单独 `on_weight_edited`。每加一种形体：头文件加 mode、cpp 加一组控件、visit 加一臂。这和已禁止的 Adapter 长接口是同一类违反 I.*。

G4.3 曾允许「第一阶段 registry 就是 Panel 内 visit」。实践证明这会继续长 mode 函数（Bezier 已加第三套）。**本方案废止该妥协。**

内核侧盒子/球的 Length、Radius **已经在 `ParameterStore`**；面板却从 Spec 反算 Max−Min，权重只活在 `BezierSpec`。UI 与再生参数是两条真相。

---

## 2. 分层（类型分发不进 Widget）

```text
IFeature / PrimitiveSpec / ParameterStore     对象真相
        ↓
Describe / Apply  （无 Qt，与 Spec 同层）     字段表
        ↓
ISceneService::SpecFor / SetPrimitive         已有门面，虚表不涨
        ↓
PropertyPanel                                 只渲染 PropertySheet
```

| 层 | 允许知道 | 禁止 |
|----|----------|------|
| `BoxSpec` / `BezierSpec` | 自己的字段 | Qt |
| `Describe` / `Apply` | `PrimitiveSpec` visit **一处** | `QWidget`、`PropertyPanel` |
| `ISceneService` | Guid、`SpecFor`、`SetPrimitive` | `SetBoxParams`、`PropertySheetForBox` |
| `PropertyPanel` | `PropertySheet`、`PropertyKind`、`set_enabled` | `BoxSpec`、`set_*_mode`、`on_dim_edited` / `on_weight_edited` |

加圆柱：variant 臂 + `Describe(CylinderSpec)` / `Apply`（治理 §6 清单 1–4）。**不**改 `ISceneService.h`，**不**改 `PropertyPanel.h`。

结果实体（Boolean / Extrude）：`SpecFor` 为空 → 身份 + 只读 Hint（`Operation: Union`），无假 LWH。不要 `set_boolean_mode`。

---

## 3. 字段表（DTO，无 Qt）

放在内核 feat（经 `api/Modeling.h` 给 Viewer），**不要**放进 `PropertyPanel.cpp`。

```cpp
enum class PropertyKind
{
    Length,
    Real,
    Integer,
    Hint,
};

enum class PropertyWidget
{
    Spin,
    SliderSpin,
    ReadOnly,
};

struct PropertyField
{
    std::string Id;      // "length" / "w0"；写回用
    std::string Label;   // 英文源文，面板 tr()
    PropertyKind Kind{PropertyKind::Real};
    PropertyWidget Widget{PropertyWidget::Spin};
    double Value{0.0};
    double Min{1.0e-4};
    double Max{1.0e9};
    double Step{0.1};
};

struct PropertySheet
{
    std::string GroupTitle;  // "Dimensions (parameters)" / "Weights"
    std::string Hint;
    bool Editable{false};
    std::vector<PropertyField> Fields;
};
```

struct 字段 **PascalCase**。`Id` 稳定、不随 i18n 变。

现有体素映射（创建过程与提交后一致）：

| Spec | 字段 Id | Widget | 写回 |
|------|---------|--------|------|
| `BoxSpec` | `length` `height` `width`（X/Y/Z 边长，原点仍 Min） | Spin | 改 `Max` |
| `SphereSpec` | `radius` | Spin | 改 `Radius` |
| `BezierSpec` | `w0`…`w{n-1}`，n = `Cvs.size()`（≤ 创建上限） | SliderSpin | `Weights` |
| Boolean | 无 Fields | — | Hint only |

不要把 CV 坐标做成属性格点阵（视口拖 CV 是另一条编辑）。权重才是面板参数。

---

## 4. Describe / Apply

与 `ApplyTransform(PrimitiveSpec)` 同类：**内核一个 visit，命令/面板共用。**

```cpp
[[nodiscard]] PropertySheet Describe(const PrimitiveSpec& spec);
[[nodiscard]] PropertySheet DescribeBoolean(boolean::BooleanOp op);
[[nodiscard]] bool Apply(PrimitiveSpec& spec, std::string_view fieldId,
                         double value);
```

- `Describe`：只读投影，不改模型。
- `Apply`：按 `Id` 改 spec；未知 id / 类型不匹配 → false。
- 面板 **禁止** 再 `std::get<BoxSpec>`。

Viewer 流程：

```text
ShowEntity
    SpecFor → Describe(spec) → 重建参数区
    无 spec 且 TypeName==Boolean → BooleanParamsFor → DescribeBoolean
    否则 → 空 Fields + Hint "No editable parameters"

编辑
    Apply(spec, fieldId, value) → SetPrimitive
    成功 → ParamsChanged（现有回调：同步 mesh / 历史）
```

`ISceneService` **不**增加 `PropertySheetFor` / `SetProperty`。已有 `SpecFor`+`SetPrimitive` 足够；多一个虚函数会变成第二种类型目录。

---

## 5. PropertyPanel 契约

**固定区（与形体无关）：** Name / Type / GUID。

**动态区：** 每次 `ShowEntity` 按 `PropertySheet.Fields` 生成行（Spin 或 Slider+Spin）。不要预埋 3 个尺寸 + 4 个权重再 setVisible。

- 无选中：现有 empty 文案。
- `Editable==false`：只读。
- Hint 显示 `sheet.Hint`。
- `retranslate_ui`：对 Label 做 `tr(source)`（源文来自 Field.Label）。
- 删除 `set_box_mode` / `set_sphere_mode` / `set_bezier_mode` 及 `m_boxParamsVisible` 等旗标。

控件实现可以很小：一个 `BuildFieldRow(PropertyField)`。SliderSpin 的 0.01–10、滑条 ×100 是 **Widget 对 Real 的默认映射**，写在面板，不写进内核 DTO（DTO 只给 Min/Max/Step）。

### 5.1 现有 private 方法怎么归类

这些 **不是** 另一套 Adapter 虚函数，但多数是类型矩阵在面板内部的续集，G7 一并拆掉。

| 现在 | 是不是按形体特化 | G7 |
|------|------------------|-----|
| `set_box_mode` / `set_sphere_mode` / `set_bezier_mode` | 是（类型目录） | **删** |
| `on_dim_edited` | 是（visit Box/Sphere 写 Max/Radius） | **删** → `OnFieldEdited(fieldId)` |
| `on_weight_edited` | 是（只认 BezierSpec 权重） | **删**，并入同一个 `OnFieldEdited` |
| `refresh_dim_hint` | 是（看 `m_boxParamsVisible` / 布尔 op） | **删**；Hint 来自 `PropertySheet.Hint` |
| `block_dim_signals` / `block_weight_signals` | 伴随预埋尺寸/权重控件 | **删**；动态行用 `m_updatingUi` 或按行 block |
| `set_enabled` | 否（有无选中 ↔ empty/form） | **留**（可改名 `ShowForm`） |
| `clear` / `ShowEntity` / `retranslate_ui` | 否 | **留** |

治理要管的是 **「按几何种类分发」**，不是「面板能不能有 private 辅助」。`set_enabled` 不必写进禁止表。`on_dim_edited` 与 `on_weight_edited` 必须进禁止表：它们是 `set_*_mode` 的写回侧，加圆柱还会再长 `on_cylinder_edited`。

写回只允许一条槽：

```text
控件 valueChanged
    → OnFieldEdited(field.Id)
    → Apply(spec, id, value) → SetPrimitive
```

不要按 Widget 种类再拆 `OnSpinEdited` / `OnSliderEdited` 去 visit Spec。Slider 与 Spin 只同步同一 Field 的 Value。

---

## 6. 禁止

| 禁止 | 原因 |
|------|------|
| `set_cylinder_mode` / `XxxPropertyPage` | 类型矩阵回到面板 |
| `on_dim_edited` / `on_weight_edited` / `on_cylinder_edited` | 写回侧类型分发；必须并成 `OnFieldEdited` |
| `refresh_dim_hint` 按形体旗标分支 | Hint 只来自 Sheet |
| 内核 `#include` Qt | 分层 |
| IoC 插件式属性工厂 | 3 个体素不需要；禁止夹带 Hypodermic |
| `ISceneService::SetBoxParams` | 治理 §4 |
| 通用反射 / Qt 属性浏览器 | 过重 |
| 把 Extrude 草图+距离塞进 `PrimitiveSpec` 只为了出面板 | 治理 §2.1；结果体 Hint 即可 |

---

## 7. 与 ParameterStore（二期，不阻塞 G7）

盒子/球创建时已 `store.Add(...Length / Radius)`。长期：面板数据源改为该特征的 `UserDriven` 参数，`Set` + `Rebuild`，与 `.xl` 同一套数。

G7 **仍走 SpecFor + Apply + SetPrimitive**（与现网编辑路径一致，风险小）。二期再：

- Bezier 权重登记为 Real 参数；
- `Describe` 可改为扫 `ParameterStore`（Kind → Widget）；
- Spec 只留给 Copy / `ApplyTransform`。

不要在 G7 同时改再生与面板，避免两处 visit 语义分叉。

---

## 8. 验收

- `PropertyPanel.h` 无 `set_*_mode`、无 `on_dim_edited` / `on_weight_edited`、无 `BezierSpec` / `BoxSpec`。
- 选盒子改长宽高、选球改半径、选贝塞尔改权重：行为与改前一致，走 `SetPrimitive`。
- 选融合体：无 LWH/半径/权重，Hint 为运算名。
- 加一个假想 `CylinderSpec` 只改 `Describe`/`Apply` + variant 时，清单上 **没有** PropertyPanel。
- `Describe`/`Apply` 有 GoogleTest（改 Box 边长、Bezier `w1`、未知 id 失败）。
- `ISceneService.h` 虚表相对 G4 **零 diff**。

---

## 9. 文件落点

| 文件 | 职责 |
|------|------|
| `kernel/include/brep/feat/PropertySheet.h` | DTO + `Describe` / `Apply` 声明 |
| `kernel/src/feat/PropertySheet.cpp` | visit 实现 |
| `api/Modeling.h` | include 该头（Viewer 合法） |
| `apps/viewer/PropertyPanel.*` | 只渲染 Sheet |
| `tests/kernel/TestPropertySheet.cpp` | Describe/Apply |
| 本规格 + 实施计划 | 契约 |

`viewer_adapter` 不承担字段投影，避免 Adapter 再长一摊 visit。
