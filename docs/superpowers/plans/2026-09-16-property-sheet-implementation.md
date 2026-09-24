# 属性面板 PropertySheet — 实施计划

> **设计规格：** [`docs/superpowers/specs/2026-09-16-property-sheet-design.md`](../specs/2026-09-16-property-sheet-design.md)（§5.1 方法归类）  
> **治理：** [`docs/architecture/viewer-adapter-governance.md`](../../architecture/viewer-adapter-governance.md) §9（G7）  
> **前置：** Adapter 治理 G4.1/G4.2 已完成（`SpecFor` / `SetPrimitive`、瘦 `SceneObject`）  
> **日期：** 2026-09-16

**Goal：** 面板不再按形体分发。内核 `Describe`/`Apply` 投影 `PrimitiveSpec`；`PropertyPanel` 只渲染 `PropertySheet`，写回只走 `OnFieldEdited(fieldId)`。

**非目标（禁止夹带）：** `ISceneService` 新虚函数、Hypodermic 属性插件、ParameterStore 作为面板数据源（二期）、圆柱体素、NURBS 属性、抽壳。

---

## 0. 开工条件

**满足。** G2 `AddPrimitive`、G4 `SetPrimitive` 已在线上。面板仍按形体预埋控件。

现状（`PropertyPanel.h` private，规格 §5.1）：

| 函数 | G7 |
|------|-----|
| `set_box_mode` / `set_sphere_mode` / `set_bezier_mode` | 删 |
| `on_dim_edited` / `on_weight_edited` | 删 → `OnFieldEdited` |
| `refresh_dim_hint` | 删（Hint 来自 Sheet） |
| `block_dim_signals` / `block_weight_signals` | 删 |
| `set_enabled` | **留**（有无选中） |
| `clear` / `ShowEntity` / `retranslate_ui` | 留 |

```powershell
cmake --build cmake-build-mingw-debug --target brep_test_property_sheet brep_viewer
ctest --test-dir cmake-build-mingw-debug -C Debug --output-on-failure -R "PropertySheet|SceneAdapter"
```

---

## 里程碑

| PR | 片 | 可独立合并 | 验收 |
|----|----|------------|------|
| 1 | **P1** DTO + Describe/Apply + 单测 | 是（无 UI） | `ctest -R PropertySheet` |
| 2 | **P2** PropertyPanel 改吃 Sheet | 依赖 P1 | 盒子/球/贝塞尔/布尔改参与现在一致 |
| 3 | **P3**（可选）ParameterStore 对齐 | 另开 | 不进本计划 |

推荐 **P1+P2 同一 PR**：字段 Id 与面板一次对齐，避免两套 visit 并存。

---

## P1 — 内核字段表

**Files：**

- 新 `kernel/include/brep/feat/PropertySheet.h`
- 新 `kernel/src/feat/PropertySheet.cpp`
- `cmake/BrepFeat.cmake`（`PropertySheet.cpp` 加入 `brep_feat`）
- `kernel/include/api/Modeling.h`（`#include "brep/feat/PropertySheet.h"`）
- 新 `tests/kernel/TestPropertySheet.cpp`；`tests/CMakeLists.txt` 新 target `brep_test_property_sheet`（链 `brep` / `brep_feat`，与其它 feat 测一致）

### P1.1 DTO

规格 §3：`PropertyKind` / `PropertyWidget` / `PropertyField` / `PropertySheet`。字段 PascalCase。头文件 **禁止** Qt。

### P1.2 Describe / Apply

```cpp
[[nodiscard]] PropertySheet Describe(const PrimitiveSpec& spec);
[[nodiscard]] PropertySheet DescribeBoolean(boolean::BooleanOp op);
[[nodiscard]] bool Apply(PrimitiveSpec& spec, std::string_view fieldId,
                         double value);
```

visit 必须覆盖 `PrimitiveSpec` 全臂（`static_assert` 与 `ApplyTransform` 相同）。

| 臂 | GroupTitle | Fields | Apply |
|----|------------|--------|-------|
| `BoxSpec` | `Dimensions (parameters)` | `length` `height` `width`（X/Y/Z 边长，Min 不动） | 写 `Max` |
| `SphereSpec` | 同上 | `radius` | 写 `Radius` |
| `BezierSpec` | `Weights` | `w0`… 按 `Cvs.size()`，Widget=SliderSpin | 扩 `Weights`，值 `> 0` |
| 未知 `fieldId` | — | — | `false`，spec 不变 |

`DescribeBoolean`：`Editable=false`，Fields 空，Hint：

- Union → `Operation: Union (Fuse)`
- Subtract → `Operation: Subtract (Cut)`
- Intersect → `Operation: Intersect (Common)`

无 spec、非 Boolean：面板自己填 Hint `No editable parameters`（不必内核函数）。

有可编辑字段时 Hint：`Parameter-driven · edits regenerate the model`。

Label 英文源文与现 `.ts` 对齐：`Length (X)` / `Width (Z)` / `Height (Y)` / `Radius` / `w0` `w1`…（Label 已展开，面板 `tr(label)`）。

### P1.3 测试

- Box：三个 Length；Apply `length=2` 后边长为 2，Min 不变。
- Sphere：Apply `radius`。
- Bezier 2 CV：两个 SliderSpin；Apply `w1=2`。
- Apply `"nope"` → false。
- `DescribeBoolean(Subtract)` Hint 非空、Fields 空。

**P1 DoD：** 无 Qt；不改 `PropertyPanel` 也能编过。

---

## P2 — 面板只渲染 Sheet

**Files：** `apps/viewer/PropertyPanel.h` / `.cpp`  
**不改：** `ISceneService.h`

### P2.1 删除（类型分发 + 预埋控件）

| 删 | 替换 |
|----|------|
| `set_box_mode` / `set_sphere_mode` / `set_bezier_mode` | 动态行 |
| `on_dim_edited` / `on_weight_edited` | `OnFieldEdited(std::string_view fieldId)` |
| `refresh_dim_hint` | `m_hintLabel->setText(tr(sheet.Hint))` |
| `block_dim_signals` / `block_weight_signals` | `m_updatingUi`（或按行 block） |
| `m_lengthSpin` / `m_widthSpin` / `m_heightSpin` / `m_radiusSpin` 及对应 Label | `BuildFieldRow` |
| `m_weightsGroup` + 4 套 slider/spin | 同上 |
| `m_boxParamsVisible` / `m_sphereParamsVisible` / `m_bezierParamsVisible` / `m_bezierCvCount` / `m_booleanOp` | `m_sheet` |

禁止再为新形体加 `on_xxx_edited`。Slider 与 Spin **不要** 拆成 `OnSpinEdited` / `OnSliderEdited` 去 visit Spec。

### P2.2 保留

`SetAdapter`、`SetParamsChangedCallback`、`clear`、`ShowEntity`、`retranslate_ui`、`set_enabled`（可改名 `ShowForm`）、身份三行、empty label、`m_updatingUi`。

目标 private 面（示意，PascalCase 与现文件混用时 **新函数用 PascalCase**，与 viewer-itool / 项目函数风格一致）：

```cpp
void SetEnabled(bool enabled);          // 原 set_enabled
void ClearParamRows();
void RebuildParamRows(const PropertySheet& sheet);
QWidget* BuildFieldRow(const PropertyField& field);
void OnFieldEdited(std::string_view fieldId);
```

头文件 **禁止** 出现：`BoxSpec`、`BezierSpec`、`set_*_mode`、`on_dim_edited`、`on_weight_edited`。

### P2.3 ShowEntity / 写回

```text
身份（Name / TypeName / Guid）← 现逻辑
spec = SpecFor(...)
if spec: sheet = Describe(*spec)
else if TypeName=="Boolean": sheet = DescribeBoolean(op)
else: sheet = { Hint: "No editable parameters", Editable:false }
SetEnabled(true)
RebuildParamRows(sheet)

valueChanged:
    if m_updatingUi: return
    OnFieldEdited(field.Id)
        copy spec → Apply(spec, id, value) → SetPrimitive → ParamsChanged
        Apply 失败不写回
```

`RebuildParamRows`：清掉参数区旧行，按 `Fields` 重建；设 `GroupTitle`；Hint 用 `sheet.Hint`。SliderSpin 用 Field 的 Min/Max/Step；滑条整数映射（×100）留在 Widget，不进内核 DTO。

### P2.4 i18n

`Field.Label` / `GroupTitle` / `Hint` 走 `tr(QString::fromStdString(s))`。现有 `xcad_zh_CN.ts` 源文尽量不改。

### P2.5 手工验收

- 选盒子改 X 边长，再生，Undo 还原。
- 选球改半径。
- 选 3 CV 贝塞尔：3 个权重滑条，无第 4 行。
- 选融合体：无尺寸/权重，Hint 为运算名。
- 无选中：No selection。
- 改权重时 Slider 与 Spin 同步，且只打一次 `SetPrimitive`。

**P2 DoD：**

```text
PropertyPanel.h 无 BoxSpec / BezierSpec / set_*_mode / on_dim_edited / on_weight_edited
ISceneService.h 相对本 PR 零 diff
rg "set_box_mode|set_sphere_mode|set_bezier_mode|on_dim_edited|on_weight_edited" apps/viewer  为空
```

---

## P3 — 停放（ParameterStore）

规格 §7。权重进 `ParameterStore`；面板改为列 `UserDriven` 参数。G7 不改 `BoxFeature::Rebuild`。

---

## 约束

- 4 空格 Allman；DTO PascalCase；Viewer 只 `api/*`。
- 禁止为面板加 Adapter 虚函数。
- 禁止 IoC 注册 `IPropertyEditor`。
- `set_enabled` 不是治理对象，不必删。

---

## 回滚

只回滚 `PropertySheet.*` + `PropertyPanel.*` + `TestPropertySheet` + CMake 列表。不要动 `SetPrimitive` / 特征再生。
