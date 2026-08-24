# brepkernelstudy — 项目 C++ 风格（覆盖 Google Style）

与 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) 冲突时，**以本文为准**。

## 语言与构建

- **C++20**
- 扩展名：**`.h` / `.cpp`**
- Include guard：**`#pragma once`**
- 编译器：MinGW GCC、MSVC

## 命名

| 元素 | 风格 | 示例 |
|------|------|------|
| 类 / 结构体类型 | **PascalCase（大驼峰）** | `BooleanPipeline`, `Body` |
| 函数 | **PascalCase（大驼峰）** | `Evaluate`, `RegisterBody` |
| 局部变量 / 参数 | **camelCase（小驼峰）** | `bodyCount`, `startIndex` |
| **类成员变量** | **`m_` + camelCase（小驼峰）** | `m_evaluator`, `m_bodyGuid`, `m_document` |
| struct / DTO 数据成员 | **PascalCase（大驼峰，强制）**，无 `m_` | `Ok`, `Failed`, `Min`, `Max` |
| 接口 | `I` + PascalCase | `IBooleanEvaluator` |
| 枚举类成员 | PascalCase | `BooleanOp::Subtract` |
| 常量 | `k` + PascalCase | `kMaxRetries` |
| 命名空间 | `brep::`, `brep::viewer::` | — |
| **文件名** | **PascalCase（大驼峰）** | `Part.cpp`, `BooleanPipeline.h`, `TestBoxBoolean.cpp` |
| **头文件扩展名** | **`.h` only**（不用 `.hpp`） | `Model.h`, `api/Core.h` |
| **源文件扩展名** | **`.cpp`** | `Builder.cpp` |
| 宏 | UPPER_CASE | `BREP_INFO` |
| 工厂函数 | `Make*` / `Create*` | `MakeDefaultBooleanEvaluator()` |

### 类成员变量（强制）

**凡 `class` 的实例数据成员**（`private` / `protected`，含 PIMPL 持有物）必须：

1. 前缀 **`m_`**
2. 后缀名 **camelCase（小驼峰）**
3. **禁止** PascalCase（如 ~~`m_BodyGuid`~~、~~`m_Evaluator`~~）
4. **禁止** 无 `m_` 的 snake_case / 裸名（如 ~~`evaluator_`~~、~~`body_guid_`~~）

```cpp
class BoxFeature final : public IFeature
{
    // ✅
    Point3d m_origin{};
    param::ParameterId m_length{};
    Guid m_bodyGuid{};

    // ❌ 类成员不要用 PascalCase
    // Point3d m_Origin{};
    // Guid BodyGuid{};

    // ❌ 不要用 snake_case / 尾随下划线
    // Point3d origin_;
    // Guid body_guid_;
};
```

**与 struct / DTO 的边界**（勿混用）：

| 场景 | 成员命名 | 示例 |
|------|----------|------|
| `class` 封装状态 | `m_` + camelCase | `Part::m_model` |
| `struct` 纯数据 / API DTO | PascalCase，无 `m_` | `RegenResult::Ok`, `BoxSpec::Min` |
| 函数参数 / 局部变量 | camelCase，无 `m_` | `origin`, `axisDir`, `outerUv` |
| `class` 继承 `IObject` 且参数/返回需 `Guid` 类型 | 写 **`brep::Guid`** | `Part::FindBody(const brep::Guid&)` |
| Topology 图实体公开字段 | PascalCase（批次 26） | `Edge::V0`, `CoEdge::Next` |

> **struct / DTO 字段禁止 snake_case**（如 ~~`ok`~~、~~`failed`~~、~~`message`~~、~~`status`~~、~~`dof`~~）。新代码与本次修改必须用大驼峰。

```cpp
// ✅ struct 字段 — PascalCase
struct RegenResult
{
    bool Ok{true};
    FeatureId Failed{};
    std::string Message;
};

// ❌ 禁止
struct RegenResult
{
    bool ok{true};
    FeatureId failed{};
    std::string message;
};
```

> 迁移脚本只改 **struct 字段** 与 **函数名**；**不会**把参数/局部变量改成 PascalCase。类成员若被误改成 PascalCase，应改回 `m_` + camelCase。

**已落地类型（bool 模块）**：`BooleanResult` 成员 `OutputBody`/`Mode`/`Diagnostics`/`Ok()`；`FaceClassification::TargetFace`；避免与类型名 `Body`/`Face`/`Model` 同名。

**已落地类型（Part / Document）**：`Part::AddBox()`/`FindBody()`/`Model()`/`BooleanEvaluator()`/`FeatureHistory()`/`Regenerate()`/`RemoveFeature()`；成员 `m_model`/`m_document`/`m_booleanEvaluator`；`Document::Create()`/`AddPart()`/`MainPart()`/`Registry()`/`MarkDirty()`。

**已落地类型（Geometry）**：`Curve::Kind()`/`Eval()`/`Tangent()`/`Domain()`；`Surface::Kind()`/`Eval()`/`Normal()`/`ParamOf()`；`LineCurve::SetLength()`/`Length()`/`Origin()`/`Direction()`；`SphereSurface::Center()`/`Radius()`；`Point::Xyz()`/`SetXyz()`。

**已落地类型（viewer DocumentHistory）**：`Push()`/`Clear()`/`CanUndo()`/`CanRedo()`/`UndoLabel()`/`RedoLabel()`/`Undo()`/`Redo()`（与内核 `feat::FeatureHistory` 分离）。

**已落地类型（viewer commands）**：`ICommand::Id()`/`Title()`/`Kind()`/`CanExecute()`/`Execute()`/`MakeTool()`；`ITool::OnStart()`/`OnMousePress()`/`Prompt()`/`IsFinished()`/`Result()` 等；`CommandManager::Run()`/`History()`/`HasActiveTool()`；`CommandRegistry::RegisterCommand()`/`Create()`/`Ids()`；`CommandResult::Ok()`/`Failed()`/`Succeeded()`。

**已落地类型（Topology）**：`Vertex::Position()`；`Edge::Start()`/`End()`/`ParamAt()`；`CoEdge::From()`/`To()`/`GetFace()`；`Loop::ForEachCoedge()`/`CoedgeCount()`/`IsClosed()`；`Face::OuterLoop()`/`OuterLoops()`/`InnerLoops()`/`NormalAt()`；`Shell::FaceCount()`；`Body::Kind()`/`OuterShell()`；图字段见上「批次 26」。

**已落地类型（ObjectRegistry）**：`Add()`/`Remove()`/`Clear()`/`Find()`/`FindAs()`/`Size()`。

**已落地类型（Model）**：`Model::Bodies()`/`RemoveBody()`/`MakePoint()`/`MakeLine()`/`MakeCircle()`/`MakeLine2d()`/`MakePlane()`/`MakeSphereSurface()`/`MakeCylinderSurface()`/`MakeVertex()`/`MakeEdge()`/`MakeCoedge()`/`MakeLoop()`/`MakeFace()`/`MakeShell()`/`MakeBody()`；静态 `LinkLoop()`/`PairPartners()`/`AttachEdgeToVertices()`；`NextId()`。

**已落地类型（FeatureHistory）**：`Record()`/`ApplyAndRecord()`/`Undo()`/`Redo()`/`CanUndo()`/`CanRedo()`/`Clear()`；`FeatureTransaction` 字段 `Kind`/`Feature`/`FeatureType`/`Box`/`Op`/`TargetFeatureId`/`ToolFeatureId` 等。

**已落地类型（feat / param / Builder）**：`IFeature::Rebuild()`/`TypeName()`/`BodyGuid()`；`FeatureId::IsValid()`；`FeatureTree::Append()`/`Find()`/`FindByBody()`/`MarkAllDirty()`；`RegenResult::Ok`/`Failed`/`Message`；`ParameterStore::Get()`/`Set()`/`AddWithId()`；`MakeBox()`/`MakeSphere()`；特征类 `BoxFeature::Create()`/`ToSpec()`/`LengthId()`。

**已落地类型（IObject / 身份成员）**：`IObject::Guid`/`Name`；`Guid::IsValid()`/`Generate()`/`FromString()`/`ToString()`/`Bytes()`；`Named::Id`/`Name`/`Label()`；`Parameter::Kind`/`Value`/`UserDriven`；viewer `SceneObject`/`BodyRef`/`FeatureRef`。

**已落地类型（Assembly / Sketch）**：`Occurrence::PartGuid`/`Transform`；`Mate::Id`/`Kind`/`FaceRefA/B`；`Assembly::AddOccurrence()`/`Find()`；`Sketch::AddPoint()`/`AddConstraint()`/`Frame()`。

**已落地类型（Kernel DTO + 自由函数）**：`Material::AlbedoPath`；`Plane::Origin`/`ToWorld()`；`SnapCandidate::Kind`/`Point`/`BodyGuid`；`TessellateBody()`/`ValidateBody()`/`SaveXl()`/`QuerySnapCandidates()` 等；**批次 18–21**：`ExtractProfile()`/`Extrude()`/`ClassifyPointInPrism()`/`LastXlError()`；`MeshVertex::Position/Normal/Uv`、`TriangleMesh::Vertices/Indices`；`BoxSpec::Min/Max`、`SphereSpec::Center/Radius`、`Profile2d::Outer/Holes`、`ExtrudeSpec::Profile/Plane/Distance`；`ValidationReport::Ok()`/`Error()`/`Issues`；`XlSaveResult::Ok`/`Error`。

**已落地类型（Topology 图字段，批次 26）**：`Vertex::Point/Edges/Tolerance`；`Edge::Curve/V0/V1/T0/T1/Radial`；`CoEdge::Edge/Sense/Next/Prev/Partner/Pcurve/Loop`；`Loop::Face/Type/First`；`Face::Surface/Sense/Loops`；`Shell::Faces/Closed`；`Body::Type/Shells`。

**已落地类型（Guid / Math，批次 26）**：`Guid::Generate()`/`Nil()`/`FromString()`/`FromBytes()`/`ToString()`/`Bytes()`；`Vector3d`/`Point3d`/`Point2d` 私有 `m_data`。

**已落地类型（Viewer）**：`RenderCache`/`InputState`/`CommandContext` 字段 PascalCase；`ecs::InputOnPress()`/`PickRenderable()`；`ScreenToRay()`；`World::SyncPartBodies()`/`CreateCamera()`；`MainWindow::SetupMenus()` 等。

> 完整分批记录：`docs/architecture/style-migration-plan.md`

```cpp
class BooleanPipeline
{
public:
    [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model);

private:
    std::shared_ptr<IBooleanEvaluator> m_evaluator;
};

struct SelectionEntry
{
    Guid EntityGuid;
    int FaceIndex;
};

void Part::RegisterBody(Body& body)
{
    int registeredCount = 0;
    // ...
}
```

## 格式

| 条目 | 本项目 |
|------|--------|
| 缩进 | **4 空格一级（强制）** — **禁止 Tab**、**禁止 2 空格** |
| 括号 | **Allman（强制）** — `{` 在声明**下一行**；根目录 `.clang-format` 已设 `BreakBeforeBraces: Allman` |
| 行宽 | 80 字符（URL、长 `#include` 可例外） |
| 指针/引用 | `Type* ptr`、`Type& ref` |

### 缩进层级

每一级嵌套 **+4 空格**（与 `.clang-format` 的 `IndentWidth: 4` 一致）：

1. **namespace 内顶层** — `class` / `struct` / `enum` 不额外缩进
2. **类访问说明符** — `public:` / `private:` 与类 `{` 同级
3. **类成员 / 函数声明** — 相对 `{` 缩进 4 空格
4. **函数体** — 相对函数声明再缩进 4 空格
5. **控制流嵌套** — `if` / `for` / `while` 每嵌套一层再 +4 空格

```cpp
namespace brep::feat
{

class BoxFeature final : public IFeature
{
public:
    [[nodiscard]] FeatureId Id() const override
    {
        return m_id;
    }

private:
    FeatureId m_id{};
};

}  // namespace brep::feat

void Part::RegisterBody(Body& body)
{
    int registeredCount = 0;
    if (body.IsValid())
    {
        registeredCount++;
    }
}
```

- 存量代码若为旧风格（snake_case 文件名、`evaluator_`、`.hpp`、**2 空格**、同行 `{`），**全项目迁移中**；**新文件与本次修改的代码必须 4 空格 + Allman**
- **文件名 / 头文件后缀迁移（正确方向）**：`snake_case.hpp` → `PascalCase.h`，`snake_case.cpp` → `PascalCase.cpp`；运行 `python scripts/migrate_style.py`，再运行 `python scripts/fix_includes.py` 统一 `#include` 路径
- **禁止**将 include 或文件名改回 `.hpp` / snake_case（`fix_includes_to_hpp.py` 已废弃）
- **Allman 迁移**：空体块（`namespace`/`class`/行末仅 `{`）可运行 `python scripts/format_allman_braces.py`；含行内语句的 `{ ... }` 需安装 LLVM `clang-format` 后运行 `python scripts/format_all.py`
- **类成员迁移**：尾随下划线 `foo_` → `m_foo`（camelCase）运行 `python scripts/refactor_trailing_underscore_to_m_members.py`（含 `.h`/`.hpp`/`.cpp`）

## 头文件

- 自包含；Include What You Use
- Viewer 生产代码仅 `api/*`（ADR 0002）

## 异常与 RTTI

- Kernel 热路径不用异常；`BooleanResult` / `bool` + diagnostics
- Viewer / Qt 可随框架惯例；不作跨模块常规控制流

## 模块边界

- Kernel 不链接 Hypodermic（ADR 0007）
- 内部头：`kernel/internal/`（PRIVATE）

## 相关 ADR

- ADR 0002：`api/*` 分级
- ADR 0006：Boolean Pipeline
- ADR 0007：Hypodermic（Viewer only）
