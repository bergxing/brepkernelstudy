# 贝塞尔曲线命令 — 交互与绘制方案

**日期：** 2026-08-27  
**修订：** 2026-09-16（渐进次数、右键确定/取消、绘制 Undo、菜单光标）  
**状态：** 已实现  
**代码：** [`CreateBezierTool`](../../../apps/viewer/commands/tools/CreateBezierTool.cpp)、[`ITool`](../../../apps/viewer/commands/ITool.h)  
**关联：** [ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)（显示）、[NURBS 建模路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md) P0/P2、[Adapter 治理](../../architecture/viewer-adapter-governance.md)、[实现任务](../plans/2026-08-27-bezier-curve-implementation.md)

**Goal：** Viewer 里用一条交互命令画 **一段有理贝塞尔 Wire**。最多 4 个控制点：2 点直线 → 3 点抛物线 → 4 点三次；预览与提交走 `EdgeMesh` / Vulkan 线框，精确几何留在 `Model`。

---

## 1. 主流软件怎么做（取舍）

| 软件 | 命令 / 工具 | 用户在点什么 | 精确几何 | 本方案是否照搬 |
|------|-------------|--------------|----------|----------------|
| **AutoCAD** | `SPLINE`，CV 模式 | 连续点控制顶点，Enter 结束；阶数 1–10 | NURBS（Bezier 是无内节点的特例） | **交互骨架**（吸附、撤点、Enter）；本命令 **最多 4 CV** |
| **Rhino** | Control Point Curve | 连续 CV，Enter 结束 | NURBS | 同上；任意个 CV / 升阶放到后续「控制点样条」 |
| **Fusion / SW** | Control Point Spline / Style Spline | CV 折线，曲线不过点（除端点） | NURBS / Bezier | **语义**：端点过曲线，中间 CV 是拉手柄不是插值点 |
| **Illustrator / Inkscape** | Pen | 锚点 + 两边手柄，多段 G1 | 分段三次 Bezier | **不做** Pen（手柄编辑、多段拼接）；那是另一条命令 |
| **CATIA GSD** | Bezier | 显式 Bezier，点数 = 阶数+1 | Bezier | **次数模型**：n 个 CV → 次数 n−1 |

**本仓库选定：AutoCAD/Rhino 的「控制点」语义 + CATIA 的「一段 Bezier、次数 = CV 数 − 1」，并允许中途结束（2 / 3 点也是合法曲线）。**

不选 Pen 工具：状态机复杂（按下拖手柄、C1 连续性），且草图求解器还没有样条实体。  
不选 Fit 插值样条（SW Fit Spline）：过型值点要解线性系统，和「Bezier」名称不符。

Bezier ⊂ NURBS：4 个 CV、三次时节点 `[0,0,0,0, 1,1,1,1]`。内核用有理 de Casteljau（任意 2–4 个 CV + 权重），**不必先做完整 `NurbsCurve` 节点算法**。不修订 ADR 0009 的「先显示后求交」；本命令是 **一条空间曲线特征**，不是 NURBS 布尔。

---

## 2. 产品范围

### 做

- 命令 `part.create_bezier`：逐点拾取控制点，最多 4 个；第 4 点收下后自动提交。
- 渐进次数：2 CV 直线、3 CV 二次、4 CV 三次；名称分别为 `line` / `parabola` / `bezier`。
- AccuSnap：与球/立方体相同（表面、端点、地面、已有 Wire 边）。
- 实时预览：曲线折线 + 控制多边形 + 已定点与光标十字。
- 提交：`BodyType::Wire` + `BezierCurveFeature`，特征可撤销；Copy 走 `BezierSpec` → `ToPrimitiveSpec`（禁止 `BezierSpecFor`）。
- 创建后：属性面板调权重（文档 Undo；面板实现见 [PropertySheet G7](./2026-09-16-property-sheet-design.md)）；视口拖 CV（另路径，见 §4.7）。
- 绘制：`ExtractEdges` 采样曲线。Wire **不贴材质**（无三角面；样式以后再加）。

### 不做（本命令）

- 任意个 CV（>4）、升阶、Fit 插值。
- 多段复合 Bezier / Illustrator Pen / 创建过程中拖手柄。
- 草图二维 Bezier（`Sketch` 仍只有 Point/Line/Circle/Arc）。
- 用这条曲线拉伸、扫掠、裁剪、求交。
- `ISceneService::BezierSpecFor`；曲面 Bezier patch。

---

## 3. 命令

| 项 | 值 |
|----|-----|
| id | `part.create_bezier` |
| 菜单 / Ribbon | Modeling → Create Bezier Curve… |
| 快捷键 | 无默认（避免占 Ctrl+B 立方体）；命令面板可搜 Bezier |
| 类型 | `CommandKind::Interactive` → `CreateBezierTool` |
| 视口选择 | `AllowsViewportSelection() == false`（创建时不抢点选） |
| 绘制 Undo | `OwnsUndoRedo() == true`（见 §4.5） |
| i18n | `QCoreApplication::translate("CreateBezierTool", …)` + `xcad_zh_CN.ts` |

菜单英文源文 / 中文：

- `Create Be&zier Curve…` / 对应 `.ts`
- Tooltip：`Four control points: cubic Bézier (ESC cancel)` / `四点三次贝塞尔（ESC 取消）`

治理：`BezierSpec` 进 `PrimitiveSpec`；Adapter 走 `AddPrimitive`。禁止 `BezierSpecFor`。

---

## 4. 交互逻辑（现行契约）

对齐 `CreateSphereTool` 的 AccuSnap / `SetPreviewEdges`；步进用嵌套 `enum class Step`（`PickFirst` / `PickNext`），禁止 `int m_step`。

最大控制点数 `kMaxCvs = 4`。已收下的点存在 `m_cvs`；光标候选为 `m_hover`。

### 4.1 状态机

```text
                    左键 / 右键「确定」
[PickFirst] ──────────────────────────────► [PickNext] ──(第 4 点)──► commit ──► 结束
   │  0 CV                                     │  1–3 CV
   │                                           │
   │  ESC / 取消                               │  左键 / 「确定」：收下当前点，继续
   │  （放弃）                                  │  ESC / 取消：≥2 CV 则提交并退出
   │                                           │                 <2 CV 则放弃
   │                                           │  Ctrl+Z / Backspace：丢掉最后一点
   │                                           │  Ctrl+Y：重做刚丢掉的点
   └── 收第 1 点后进入 PickNext ◄──────────────┘
```

| 已收下 CV | 次数 | 预览（+ 光标） | 提交结果 `Name` |
|-----------|------|----------------|-----------------|
| 0 | — | 仅吸附点十字 | （不能提交） |
| 1 | 1（直线预览） | P0→hover | （取消则放弃） |
| 2 | 1 | 直线 + hover 二次预览 | `line` |
| 3 | 2 | 抛物线 + hover 三次预览 | `parabola` |
| 4 | 3 | —（立刻 commit） | `bezier` |

共线 2 点 / 退化曲线仍合法，允许提交。

### 4.2 拾取（下一步）

下列操作 **等价**：收下当前吸附点，进入下一步。

| 输入 | 行为 |
|------|------|
| **左键** | `AcceptPoint` |
| **右键菜单 → 确定** | 同上（用 **弹出菜单时的视口坐标**，不是点在菜单项上的坐标） |
| **Enter / Return** | 有 hover 则 `AcceptPoint`；否则若已有 ≥2 CV 则提交 |

约束：

- 未命中表面/地面：不收点，提示 `Missed surface/ground — try another angle`。
- 与上一点距离 `< 1e-6`：不收点，提示 `Points too close — pick farther`。
- 收下第 4 点：**立即 commit 并退出**，不再等 Enter / 再点确定。

状态栏：

| 步骤 | 英文源文 |
|------|----------|
| `PickFirst` | `Pick first point` |
| `PickNext` | `Pick next point` |

### 4.3 结束（取消 / ESC）

**取消** 与创建过程中的 **ESC**（菜单未打开）走 `FinishKeepOrAbort`：

| 已收下 CV | 结果 |
|-----------|------|
| ≥ 2 | **保留**：按当前点数提交（直线或抛物线），工具结束 |
| 0 或 1 | **放弃**：清预览，`CommandStatus::Cancelled` |

右键菜单 **取消**、菜单打开时按 **ESC**（菜单先关掉）与上面相同。

### 4.4 右键菜单

视口右键（无拖拽）且工具激活时，**不**弹出 Copy/Move/Delete，而弹出工具菜单：

| 项 | 英文源文 | 行为 |
|----|----------|------|
| 确定 | `Confirm` | 收下当前点并 **继续**；仅第 4 点才退出 |
| 取消 | `Cancel` | §4.3 |

菜单是 `QMenu::exec` 弹出层。必须满足 §4.8，否则项无法悬停/点中。

### 4.5 绘制 Undo / Redo（不是文档 Undo）

创建过程中工具 **占用** Ctrl+Z / Ctrl+Y / 功能区 Undo·Redo：

| 操作 | 行为 |
|------|------|
| Ctrl+Z、功能区 Undo、**Backspace** | 丢掉最后收下的 CV（`UndoStep`） |
| Ctrl+Y、功能区 Redo | 恢复刚丢掉的 CV（`RedoStep`）；若因此凑满 4 点则 commit |
| 属性面板改权重 / 已提交特征 | 仍走 `DocumentHistory`（仅工具 **结束后**） |

已无点可撤时，Undo 失败、工具不退出。

**菜单开着时的 Ctrl+Z / Ctrl+Y：** 先关掉右键菜单，再执行上表的绘制 Undo/Redo。不要把这次按键当成文档撤销，也不要留给 `QAction` 在菜单关掉后误触。

### 4.6 预览（每次 `OnMouseMove`）

已定点为前缀，光标为下一候选点。只走 `SetPreviewEdges`（空三角网）。

| 内容 | 画什么 |
|------|--------|
| 标记 | 每个已定点 + hover 的十字（`MakePointMarker`） |
| 控制多边形 | 相邻 CV（含 hover）线段 |
| 曲线 | `AppendBezierCurvePreview`：`BezierCurve(prefix+hover)` 均匀 32 段 |

### 4.7 创建之后（本命令结束后）

| 能力 | 行为 |
|------|------|
| 选中 Wire | 属性面板显示与 CV 数一致的 **权重** 滑条；改动走 `SetPrimitive` + 文档 Undo |
| 视口拖 CV | 空闲时按住 CV 十字拖动改型；ESC 取消本次拖动 |
| 材质 | Wire 不加载木纹/反照率；样式以后另做 |

创建过程中的 Undo **不是** 属性滑条的文档 Undo。

### 4.8 视口弹出层（输入路由）

工具激活时主窗口在 `qApp` 上装了 eventFilter，并按全局坐标把鼠标转给工具。菜单叠在视口上时，若不让路，悬停高亮和单击都会被当成拾取。

**必须：**

1. `QApplication::activePopupWidget()` 非空时，**不要**把 MouseMove / Press / Release 当成工具拾取（`handle_tool_mouse` 直接放过）。
2. `VulkanWindow` 装在 container 上的 filter 同样在弹出层期间放过鼠标，避免 native 窗口把点击吞掉。
3. 弹出层期间 **十字光标改回箭头**；菜单关掉且仍在创建中则恢复十字。
4. ESC 在弹出层打开时 **不要** 直接 `cancel_active_tool`（否则会拆掉还在 `exec()` 里的工具）。先让菜单关掉。
5. 弹出层期间 Ctrl+Z / Ctrl+Y：关掉菜单 + 绘制 Undo/Redo（§4.5）。`ShortcutOverride` 要接住，避免功能区 `QAction` 抢快捷键。

这些规则对视口 Copy/Move/Delete 菜单同样适用。

---

## 5. 精确几何（Kernel）

### 5.1 求值

n 个 CV、次数 n−1 的有理 de Casteljau（权重缺省为 1）。三次 Bernstein 是 n=4、权重全 1 的特例：

\[
B(t)=(1-t)^3 P_0 + 3(1-t)^2 t P_1 + 3(1-t) t^2 P_2 + t^3 P_3,\quad t\in[0,1]
\]

切向用于以后延伸/打断；创建预览可不画切向。

### 5.2 类型

```cpp
struct BezierSpec {
    std::vector<Point3d> Cvs;      // 2–4 个控制点
    std::vector<double> Weights;   // 可空 = 全 1
    int Degree{3};                 // 提交时 = Cvs.size() - 1
    int SegmentCount{1};
    double Tolerance{1e-7};
    std::string Name{"bezier"};    // line | parabola | bezier
};
```

- `BezierCurve(cvs, weights)`：`Kind()==Bezier`，`Domain()=={0,1}`。
- `MakeBezierWire`：`BodyType::Wire`，一根 `Edge`，两端 Vertex 为曲线端点，`t0=0,t1=1`。无三角面。
- `BezierCurveFeature`：`TypeName "Bezier"`；`ToPrimitiveSpec` 返回 `BezierSpec`。
- `ApplyTransform` **保持 CV 个数与权重**，禁止把 2/3 点曲线强行扩成 4 点。

测试：`TestBezierCurve` — 端点、中点、有理权重；`TestApplyTransform` — 变换后 CV 数不变。

---

## 6. 绘制

GPU **不能** 画解析 Bezier（ADR 0009：精确几何 ≠ 显示折线）。

```text
BezierCurve::Eval
    ├─ 预览（Tool）     SampleBezierPolyline → EdgeMesh → SetPreviewEdges
    ├─ 提交后显示       ExtractEdges(Wire) → ECS MeshComponent.edges
    └─ 控制网           创建中始终画；完成后默认只画曲线；选中可拖 CV
```

**v1 均匀：** \( t = i/N,\ N=32 \)。  
**禁止**把显示折线写回 B-Rep。

线宽与颜色走现有边 shader。`TriangleMesh` 空；`SyncPartBodies` 对无三角的 Wire **不要** `set_material` 木纹。

---

## 7. Viewer 文件与分层

| 文件 | 职责 |
|------|------|
| `commands/tools/CreateBezierTool.*` | 状态机、预览、commit、右键菜单 |
| `commands/ITool.h` | `OnContextMenu`、`OwnsUndoRedo` |
| `commands/CommandManager.cpp` | 工具调度深度；弹出层期间不销毁工具 |
| `ui/InputRouter.cpp` | 弹出层让路、Ctrl+Z 关菜单、ESC 不拆 `exec` |
| `ui/ContextMenu.cpp` | 工具激活时把 RMB 交给 `tool_context_menu` |
| `VulkanWindow.cpp` | 弹出层让路；创建后 CV 拖动 |
| `PropertyPanel.*` | 创建后权重：G7 起由 `PropertySheet` 动态行渲染（[property-sheet-design](./2026-09-16-property-sheet-design.md)），文档 Undo |
| `BuiltinCommands.cpp` | 注册 `part.create_bezier` |
| `api/Modeling.h` + `api/Mesh.h` | 命令只 include `api/*` |

命令 **不** include `brep/bool/**`。

---

## 8. 实现切片

任务拆分仍见  
[`docs/superpowers/plans/2026-08-27-bezier-curve-implementation.md`](../plans/2026-08-27-bezier-curve-implementation.md)。  
**Bz4 交互以本文 §4 为准**（已从「固定四点再提交」改为渐进次数 + 右键确定=下一步）。

| 片 | 内容 | 状态 |
|----|------|------|
| **Bz1** | `BezierCurve::Eval` + 采样 | 已做（含有理 / 可变 CV 数） |
| **Bz2** | Wire + ExtractEdges | 已做 |
| **Bz3** | Feature + History + `.xl` | 已做 |
| **Bz4** | `CreateBezierTool` + i18n | 已做（§4） |
| **Bz5** | `SpecFor` + CopyTool | 已做 |
| **Bz6** | 选中控制网 / 拖 CV / 权重 | 已做（§4.7） |

---

## 9. 验收（交互）

- 两点 + ESC 或取消：得到直线 Wire，名称 `line`。
- 三点 + ESC：抛物线，`parabola`。
- 第四点（左键或确定）：三次曲线，`bezier`，工具退出。
- 第一点后右键 **确定**：收下第二点，**仍在命令中**。
- 右键菜单可悬停、可点；指针在菜单上是箭头，不在视口上仍是十字。
- 菜单打开时 Ctrl+Z：菜单关掉，最后一点消失，命令继续。
- 菜单打开时 ESC：菜单关掉，按 §4.3 结束。
- 创建中 Ctrl+Z 不撤销文档里刚改过的其它特征。
- 连续两点过近 / 未命中：不收点，有提示。
- Wire 提交后不刷木纹材质。

---

## 10. 后续（不在本命令）

| 能力 | 参照 | 前置 |
|------|------|------|
| n 个 CV 的三次有理 B 样条 | AutoCAD SPLINE CV、Rhino | [有理 B 样条曲线需求](./2026-09-24-rational-bspline-curve-requirements.md)（未实施） |
| Pen / 多段 G1 | Illustrator | 草图实体或 Composite 特征 |
| 曲线样式（线型/颜色） | 显示层 | 与材质解耦 |
| 作扫掠路径 | NX/SW Sweep | Wire 拾取 + SweepFeature |
