# 有理 B 样条曲线 — 需求

**日期：** 2026-09-24  
**状态：** 已实现（视口点击未手测）  
**实施：** [有理 B 样条曲线实施计划](../plans/2026-09-24-rational-bspline-curve-implementation.md)  
**参照：** [贝塞尔曲线命令](./2026-08-27-bezier-curve-command-design.md)（交互契约与 Wire 特征的已实现基线）  
**关联：** [ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)（显示，不做求交）、[NURBS 综合方案](./2026-08-24-nurbs-unified-strategy.md)、[建模 UI 路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md) P2「插入 NURBS 曲线」

**Goal：** 在已有「一段有理贝塞尔 Wire」之上，增加一条 **开放夹紧有理 B 样条（NURBS）曲线** 命令。控制点可以多于 4 个，次数固定为三次，节点由内核生成。精确几何留在 `Model`，屏幕只画采样折线。

本文只沉淀需求。不修订 ADR 0009，不新开 ADR 0011，不包含曲面、求交或布尔。

---

## 1. 和贝塞尔的关系

有理贝塞尔是有理 B 样条的特例：没有内部节点，节点在域的两端各重复 `degree + 1` 次。

| | 贝塞尔（已实现） | 有理 B 样条（本文） |
|--|------------------|---------------------|
| 控制点 | 2–4，第 4 点自动提交 | ≥ 4，Enter / 取消提交，可继续加点 |
| 次数 | `CV 数 − 1`（1 / 2 / 3） | 固定 **3** |
| 节点 | 隐式 `[0,0,0,0, 1,1,1,1]`（三次） | 夹紧均匀节点，内部节点随 CV 增加 |
| 权重 | 缺省 1，创建后可改 | 同左；权重大于 0 才是有理 |
| 端点 | 曲线过首尾 CV | 夹紧节点下同样过首尾 CV |
| 中间 CV | 拉手柄，曲线不过这些点 | 同左 |
| 求值 | 有理 de Casteljau | 有理 Cox–de Boor |

4 个 CV、权重全 1、夹紧节点 `[0,0,0,0, 1,1,1,1]` 的本曲线必须与现有三次 `BezierCurve` 在 `t ∈ [0,1]` 上一致（端点、`t = 0.5`）。已有 `part.create_bezier` **保留**，不并入本命令。

---

## 2. 主流软件怎么做（取舍）

| 软件 | 命令 / 工具 | 用户在点什么 | 精确几何 | 本需求是否照搬 |
|------|-------------|--------------|----------|----------------|
| **AutoCAD** | `SPLINE`，CV 模式 | 连续控制点，Enter 结束；阶数可设 | 夹紧 NURBS | **交互骨架**（吸附、撤点、Enter 结束） |
| **Rhino** | Control Point Curve | 连续 CV，Enter 结束 | NURBS | 同上 |
| **Fusion / SW** | Control Point Spline | CV 折线，曲线不过中间点 | NURBS | **语义**：端点过曲线，中间 CV 是拉手柄 |
| **CATIA GSD** | NURBS / Spline | 显式阶数与节点 | NURBS | 阶数固定三次；**节点不交给用户编辑** |
| **本仓库贝塞尔** | `part.create_bezier` | 最多 4 CV，点数即次数 | 有理 Bezier | 交互细则（右键、绘制 Undo、弹出层）**照搬** |

不选 Fit 插值（过型值点再反求 CV）：和「控制点」语义不一致，留给以后。  
不选周期 / 闭合样条：首版只做开放夹紧曲线。首尾 CV 重合也不自动闭合成周期曲线。  
不选 Pen（锚点 + 手柄、多段 G1）：贝塞尔需求已排除，本命令同样不做。

---

## 3. 产品范围

### 做

- 命令 `part.create_nurbs_curve`：逐点拾取控制点；至少 4 点才能提交；没有 4 点上限。
- 次数固定为 3。节点为夹紧均匀向量，长度 `CV 数 + 次数 + 1`，两端各重复 4 次，内部节点均匀分布在 `(0,1)`。
- 权重缺省为 1。创建后属性面板可改每个 CV 的权重（正数）。
- AccuSnap、预览（曲线 + 控制多边形 + 十字）、右键确定/取消、绘制 Undo、弹出层让路：与贝塞尔 §4 相同。
- 提交：`BodyType::Wire` + 一条 Edge + `NurbsCurveFeature`。Copy 走 `NurbsCurveSpec`，禁止单独的 `NurbsCurveSpecFor`。
- 绘制：`ExtractEdges` 采样。Wire 不贴材质。
- 创建后视口拖已提交曲线的 CV（与贝塞尔拖 CV 同一类路径）。

### 不做

- 有理 B 样条 **曲面**、放样、扫掠、修剪、缝合。
- 与解析曲线/曲面求交，以及任何布尔。
- 用户编辑节点、改次数、升阶、插入节点、周期样条、Fit 插值。
- 多段 G1 Pen、草图二维样条。
- 替换或删除 `part.create_bezier`。
- 把显示折线写回 B-Rep。

---

## 4. 命令

| 项 | 值 |
|----|-----|
| id | `part.create_nurbs_curve` |
| 菜单 / Ribbon | Modeling → Create NURBS Curve… |
| 快捷键 | 无默认 |
| 类型 | `CommandKind::Interactive` |
| 视口选择 | `AllowsViewportSelection() == false` |
| 绘制 Undo | `OwnsUndoRedo() == true` |
| i18n | `QCoreApplication::translate("CreateNurbsCurveTool", …)` + `xcad_zh_CN.ts` |

菜单英文源文 / 中文：

- `Create &NURBS Curve…`
- Tooltip：`Cubic rational B-spline: pick control points, Enter to finish (ESC cancel)` / `三次有理 B 样条：拾取控制点，Enter 结束（ESC 取消）`

`NurbsCurveSpec` 进入与 `BezierSpec` 相同的图元规格通道，经 `AddPrimitive` 提交。

---

## 5. 交互逻辑

对齐贝塞尔 §4。差别只有三处：次数不随点数升高到 3 以后继续变；第 4 点 **不** 自动提交；点数可以多于 4。

最大控制点数不设产品上限。实现可设一个远大于交互需要的护栏（例如 256），超出则拒绝收点并提示。

### 5.1 状态机

```text
                    左键 / 右键「确定」
[PickFirst] ──────────────────────────────► [PickNext] ──(Enter 且 ≥4 CV)──► commit ──► 结束
   │  0 CV                                     │  ≥1 CV
   │                                           │
   │  ESC / 取消                               │  左键 / 「确定」：收下当前点，继续
   │  （放弃）                                  │  Enter：≥4 CV 提交；不足 4 则提示并留下
   │                                           │  ESC / 取消：≥4 CV 提交并退出
   │                                           │                 <4 CV 放弃
   │                                           │  Ctrl+Z / Backspace：丢掉最后一点
   │                                           │  Ctrl+Y：重做刚丢掉的点
   └── 收第 1 点后进入 PickNext ◄──────────────┘
```

| 已收下 CV | 预览（+ 光标） | 提交 |
|-----------|----------------|------|
| 0 | 仅吸附点十字 | 不能 |
| 1–3 | 用 **当前点数** 的有理贝塞尔预览（与贝塞尔命令相同的观感） | 不能；ESC 放弃 |
| ≥ 4 | 三次夹紧有理 B 样条，光标作为下一候选 CV | Enter / ESC / 取消 可以提交 |

不足 4 点时预览可以仍是贝塞尔，因为此时还没有内部节点。提交结果一律是 `NurbsCurve`，不把 4 点曲线改写成 `BezierCurve`。

共线或退化曲线仍允许提交。

### 5.2 拾取

与贝塞尔 §4.2 相同：左键、右键「确定」（用弹出菜单时的视口坐标）、Enter。

| 约束 | 行为 |
|------|------|
| 未命中表面/地面 | 不收点，提示 `Missed surface/ground — try another angle` |
| 与上一点距离 `< 1e-6` | 不收点，提示 `Points too close — pick farther` |
| Enter 时 CV 少于 4 且没有 hover | 不提交，提示 `Need at least 4 control points` |
| 第 4 点及以后 | **不** 自动退出，继续拾取直到 Enter / ESC / 取消 |

状态栏：

| 步骤 | 英文源文 |
|------|----------|
| `PickFirst` | `Pick first point` |
| `PickNext` 且 CV < 4 | `Pick next point` |
| `PickNext` 且 CV ≥ 4 | `Pick next point, Enter to finish` |

### 5.3 结束、右键、绘制 Undo、弹出层

取消与 ESC 的「≥4 保留 / <4 放弃」、右键菜单两项（确定 = 收下并继续，取消 = §5.3）、绘制 Undo 与文档 Undo 的分界、菜单打开时的 Ctrl+Z / ESC、弹出层不抢拾取：全部遵循贝塞尔 §4.3–§4.5 与 §4.8。把其中的「≥2 CV」换成「≥4 CV」，「凑满 4 点则 commit」换成「Redo 不因为凑满 4 点而提交」。

### 5.4 预览

已定点为前缀，光标为下一候选点。只走 `SetPreviewEdges`（空三角网）。

| 内容 | 画什么 |
|------|--------|
| 标记 | 每个已定点 + hover 的十字 |
| 控制多边形 | 相邻 CV（含 hover）线段 |
| 曲线 | CV < 4：现有贝塞尔预览；CV ≥ 4：`NurbsCurve(prefix+hover)` 按弦高采样 |

### 5.5 创建之后

| 能力 | 行为 |
|------|------|
| 选中 Wire | 属性面板列出与 CV 数一致的权重；改动走文档 Undo。次数与节点只读 |
| 视口拖 CV | 空闲时拖动已提交曲线的 CV；ESC 取消本次拖动。拖动不改节点向量的结构，只改该 CV 坐标 |
| 材质 | Wire 不加载木纹/反照率 |

---

## 6. 精确几何（Kernel）

### 6.1 表示

开放夹紧有理 B 样条。控制点 \(P_i\)，权重 \(w_i > 0\)，次数 \(p = 3\)，节点 \(U = \{u_0 \le \cdots \le u_{n+p+1}\}\)，\(n+1\) 为 CV 个数。

\[
C(t)=\frac{\sum_i N_{i,p}(t)\, w_i P_i}{\sum_i N_{i,p}(t)\, w_i}
\]

\(N_{i,p}\) 为 Cox–de Boor。域是 `[U[p], U[n+1]]`，本需求生成的节点把这个区间归一到 **[0, 1]**。

夹紧：`U[0..p] = 0`，`U[n+1..n+p+1] = 1`。内部节点个数为 `n - p`，均匀落在 (0,1)。

| CV 数 | 内部节点 | 节点（示意） |
|-------|----------|----------------|
| 4 | 0 | `[0,0,0,0, 1,1,1,1]`（即三次贝塞尔） |
| 5 | 1 | `[0,0,0,0, 0.5, 1,1,1,1]` |
| 6 | 2 | `[0,0,0,0, 1/3, 2/3, 1,1,1,1]` |

权重缺省为 1。空权重向量与「全部为 1」等价。任一权重 ≤ 0 则规格非法，拒绝创建。

### 6.2 类型

```cpp
struct NurbsCurveSpec {
    std::vector<Point3d> Cvs;     // ≥ 4
    std::vector<double> Weights;  // 空 = 全 1；否则与 Cvs 等长且均 > 0
    std::vector<double> Knots;    // 空 = 按 §6.1 生成夹紧均匀节点
    int Degree{3};                // 本命令只接受 3
    double Tolerance{1e-7};
    std::string Name{"nurbs"};
};
```

- `NurbsCurve`：`Kind() == CurveKind::Nurbs`，`Domain() == {0, 1}`，`Eval` / `Tangent`。
- 端点：`Eval(0)` 等于第一个 CV，`Eval(1)` 等于最后一个 CV（夹紧）。
- `MakeNurbsCurveWire`：`BodyType::Wire`，一根 `Edge`，两端 Vertex 为曲线端点，参数 0 和 1。无三角面。
- `NurbsCurveFeature`：`TypeName` `"NurbsCurve"`；序列化进 `.xl`；`ApplyTransform` 只变换 CV，**权重、次数、节点不变**。
- 显式传入的 `Knots` 必须满足：非降、长度 = `Cvs.size() + Degree + 1`、夹紧、内部无超过 `Degree` 的重复（本命令生成的均匀节点重复度为 1）。否则规格非法。

求值不得通过把曲线离散成折线再插值。显示采样只用于 `EdgeMesh`。

### 6.3 与贝塞尔求值的一致性

测试必须锁定：4 CV、权重全 1、默认节点的 `NurbsCurve::Eval` 与同 CV 的三次 `BezierCurve::Eval` 在 `0, 0.25, 0.5, 0.75, 1` 处一致（容差 `1e-9`）。提高中间某个权重后，曲线应偏向该 CV，且端点仍为 `P0` / `Pn`。

---

## 7. 绘制

与贝塞尔 §6 相同：GPU 不画解析 NURBS。

```text
NurbsCurve::Eval
    ├─ 预览（Tool）     弦高采样 → EdgeMesh → SetPreviewEdges
    ├─ 提交后显示       ExtractEdges(Wire) → ECS MeshComponent.edges
    └─ 控制网           创建中始终画；完成后默认只画曲线；选中可拖 CV
```

采样用现有 `TessellationOptions` 的弦高（缺省 `0.02 *` 控制多边形包围盒对角线，并设段数上下限）。**禁止**把折线写回 B-Rep。Wire 不 `set_material`。

---

## 8. 分层

| 位置 | 职责 |
|------|------|
| `kernel` 几何 | `NurbsCurve` 求值与切向 |
| `kernel` 特征 | `NurbsCurveFeature`、`.xl`、`ApplyTransform` |
| `api/Modeling.h`、`api/Mesh.h` | Viewer 唯一入口 |
| `commands/tools/CreateNurbsCurveTool` | 状态机、预览、提交、右键菜单 |
| 属性面板 / 视口拖 CV | 创建后的权重与 CV |

命令不 include `brep/bool/**`。不把 Hypodermic 放进 kernel。

---

## 9. 验收

内核：

- 4 CV、单位权重：与三次贝塞尔在五个参数上一致。
- 5 CV、单位权重：`Eval(0)` / `Eval(1)` 为端点 CV；`Eval(0.5)` 落在曲线上而不是控制多边形的弦上（内部节点生效）。
- 一个权重大于 1：该 CV 对中段的拉力大于单位权重时的拉力；端点不变。
- 权重 ≤ 0、CV 少于 4、次数不是 3、节点长度不对：创建失败。
- `ApplyTransform` 后 CV 被变换，权重与节点原样保留。
- `.xl` 往返后 CV、权重、节点一致。

交互（实现时）：

- 三点 + ESC：放弃，不产生 Body。
- 四点 + Enter：得到名为 `nurbs` 的 Wire，工具退出。
- 第五点不会自动提交；再 Enter 后曲线带一个内部节点。
- 右键「确定」只收下当前点，不结束命令。
- 菜单可悬停；弹出层期间拾取让路；菜单打开时 Ctrl+Z 只撤最后一个 CV。
- 连续两点过近 / 未命中：不收点，有提示。
- 提交后的 Wire 没有木纹材质。

---

## 10. 仍不在本文

| 能力 | 说明 |
|------|------|
| 有理 B 样条曲面 | 路线图 P2 的另一条，需单独需求 |
| Fit 插值、周期样条、节点编辑、改次数 | 控制点三次夹紧做完再议 |
| 扫掠 / 拉伸 / 求交 / 布尔 | ADR 0009 仍排除；正式做建模 UI 时另开 ADR 0011 |
| 替换贝塞尔命令 | 两点、三点曲线继续走 `part.create_bezier` |
