# 拉伸 / 挤压 / 放样 / 扫掠 / 融合 — 三维造型特征规格

**日期：** 2026-08-31  
**状态：** 设计（未排期实现）  
**关联：** [layering.md](../../architecture/layering.md)、[ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md)（布尔）、[ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)（NURBS 显示）、[NURBS 建模路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md) P3、[NURBS 综合方案](./2026-08-24-nurbs-unified-strategy.md)、`ExtrudeFeature` / `BooleanFeature` / `BezierCurveFeature`

**Goal：** 在现有 B-Rep 特征树与 Viewer 命令体系上，分期交付 **草图驱动实体造型**（拉伸、挤压）与 **截面/路径驱动造型**（放样、扫掠、融合），使用户能由 2D 轮廓或空间曲线生成可再生的 Solid Body，并可与现有布尔组合。

---

## 1. 术语与 CAD 对照

中文机械 CAD 叫法不一，本文 **固定下列语义**，实现与 UI 英文源文与之对齐。

| 中文（本文） | 英文 / 类型名 | 主流 CAD 参照 | 说明 |
|--------------|---------------|---------------|------|
| **拉伸** | `Extrude` / **Pad** | SW 拉伸凸台、Fusion Extrude | 平面闭合轮廓沿法向 **加材**，产出新 Solid |
| **挤压** | `ExtrudeCut` / **Pocket** | SW 拉伸切除、Fusion Cut | 平面闭合轮廓沿法向 **减材**，从 **已有 Solid** 中切除 |
| **放样** | `Loft` | SW/Fusion Loft | **≥2 个截面**（闭合轮廓或 Wire）之间过渡成 Solid/Shell |
| **扫掠** | `Sweep` | SW/Fusion Sweep | **一个截面** 沿 **一条路径曲线** 扫过成 Solid/Shell |
| **融合** | `Blend` | CATIA Blend / SW 边界混合（简化） | **两条边链 / 两条 Wire 之间的曲面过渡**（G0/G1）；**已确认：不是布尔并** |
| **对称拉伸** | `Symmetric Extrude` | SW 双向拉伸、Fusion Symmetric | 以草图平面为中心，向 **法向两侧各拉一半** 深度（见 §1.1） |
| **拔模** | `Draft` | SW 拔模角、Fusion Draft | 侧壁相对拉伸方向加 **锥角**，便于脱模（见 §1.1） |

**与仓库现有名词的区分（必读）：**

| 仓库已有说法 | 含义 | 与本文关系 |
|--------------|------|------------|
| `BooleanFeature` + `boolean.union` | 两 **已有 Solid** 做 CSG 并 | UI 称 **Union/并集**；**禁止** 在中文 UI 中称作「融合」 |
| 文档里的「融合体」 | 布尔并/减/交 **结果 B-Rep** | 历史文档用语；**新 UI 改用「并集体」** 以免与 Blend 混淆 |
| `ExtrudeFeature` | 草图 + 距离 → 棱柱 Solid | 即本文 **拉伸 Pad** 的内核雏形 |
| `ops::Extrude` | 平面多边形（可带孔）拉伸 | 仅 **正向** 加材；无 Cut/Loft/Sweep |

**扫略** 为 **扫掠** 笔误，全文统一 **扫掠 (Sweep)**。

### 1.1 拉伸相关选项（对称拉伸 / 拔模）

#### 对称拉伸（Symmetric Extrude）

普通拉伸只向法向 **一侧** 延伸深度 `D`。 **对称拉伸** 以草图平面为对称面，向两侧 **各延伸 D/2**：

```text
              ┌─────┐
              │     │  ← +D/2
──────────────┼─────┼──────────────  草图平面
              │     │  ← −D/2
              └─────┘
总深度 = D（用户输入的是总高度，不是单侧）
```

- **M1 必做**：`ExtrudeSpec::Symmetric` / `ExtrudeFeature` 已存在字段，Viewer 提供 checkbox「Symmetric」。
- 预览：同时显示两侧棱柱线框。

#### 拔模（Draft Angle）

拔模在拉伸侧壁上施加相对拉伸方向的 **锥角** θ（通常 1°～5°），侧壁不再竖直，而略向外（或向内）倾斜，用于注塑/铸造脱模：

```text
无拔模（θ=0°）              有拔模（θ>0°，外扩）
    ┌───────┐                    ╱───────╲
    │       │                   ╱         ╲
    │       │                  ╱    θ     ╲
    └───────┘                 ╱___________╲
```

- **M1 不做**；侧壁由竖直棱柱变为锥台/斜面，需扩展 `ops::Extrude` 或后处理侧壁面。
- **M5 — 拔模（Draft）** 专里程碑（见 §3.5）：Pad / Pocket 可选 `draftAngle` + `pullDirection`（草图法向）+ `neutralPlane`（v1 默认草图面）。

#### 与「挤压」的区分

| | 拉伸 Pad | 挤压 Pocket |
|---|----------|-------------|
| 材料 | 加材 | 减材（需已有目标体） |
| 对称 / 拔模 | M1 对称；M5 拔模 | M2 起可继承相同选项（M5 拔模对 Cut 同样适用） |

---

## 2. 现状基线

### 2.1 内核（`brep_feat` / `brep_core`）

| 能力 | 状态 | 位置 |
|------|------|------|
| 矩形 `SketchFeature` + `ExtrudeFeature` | ✅ 可 Regenerate | `Part::AddRectangleSketch` / `AddExtrude` |
| 平面多边形拉伸（含 Inner 孔） | ✅ | `ops::Extrude` / `extrude_polygon` |
| `ExtrudeSpec::Symmetric` | ✅ 字段有；**ExtrudeFeature 待接参数**（M1） | `ops/Profile.h` |
| 拔模角（Draft） | ❌ | M5 |
| 任意草图实体（Line/Circle/Arc） | ⚠️ 数据结构有；**闭合轮廓提取** 仅线集启发式 | `sketch::Sketch`、`ExtractProfile` |
| 空间 Bezier Wire | ✅ | `BezierCurveFeature` |
| 布尔 Union/Subtract/Intersect | ✅ 通用 Pipeline（解析面为主） | `BooleanFeature`、`IBooleanEvaluator` |
| NURBS 曲面 Solid 构建 | ❌ | 路线图 P3 Gate |
| Trim / Sew / Loft / Sweep 拓扑算子 | ❌ | 待专 ADR |
| 变半径 / 面间 Blend | ❌ | 远期 |

### 2.2 Viewer

| 能力 | 状态 |
|------|------|
| 交互创 Box / Sphere / Bezier | ✅ |
| 菜单/ Ribbon / 右键布尔三操作 | ✅（与差集同一 `BooleanTwoBodyTool`；[交互规格](./2026-09-17-boolean-two-body-interaction.md)） |
| 草图编辑器、拉伸向导 | ❌ |
| 放样/扫掠/融合命令 | ❌ |

### 2.3 架构约束（继承 AGENTS / layering）

1. **长期身份用 `Guid`**；命令与 ECS 只存 `BodyRef`，执行前 `FindBody`。
2. Viewer 生产代码经 **`api/*` + Adapter** 调 `Part`，不直接改 `Model` 池拓扑。
3. 新特征实现 **`IFeature`**，经 `FeatureTree::Append` → `Regenerate`。
4. **显示与 B-Rep 分离**：预览用 `EdgeMesh` / 临时 mesh；提交后 `force_rebuild`。
5. 拉伸/融合等 **结果体** Copy 走 `DuplicateBody`；Move 就地变换见 G6（`Placement` 挂特征），与 [Move 交互 spec](./2026-08-27-move-command-interaction.md) 一致。

---

## 3. 分期范围

按 **内核可交付性** 切四期；每期可独立验收，不要求一次做完五种特征。

```text
M1 拉伸 Pad（Viewer + 草图 v1 + 对称拉伸） ──► ExtrudeFeature / ops::Extrude
M2 挤压 Pocket（切除拉伸）                 ──► Extrude + Boolean Subtract
M3 扫掠 Sweep（v1 限定）                  ──► 平面截面 + 折线/Bezier 路径
M4 放样 Loft + 融合 Blend（v1 限定）        ──► 直纹过渡；Blend = 曲面过渡（已确认）
M5 拔模 Draft（Pad / Pocket 可选）         ──► 侧壁锥角
```

### 3.1 M1 — 拉伸（Pad）

**目标：** 用户在 Viewer 内建 **平面草图闭合轮廓** → 指定深度 → 生成 **Solid**，进特征树，可改深度 Regenerate。

**做：**

- 命令 `part.extrude_pad`（交互 `ExtrudePadTool`）。
- 草图 v1：**矩形**（两点对角）或 **多段线闭合**（顶点序列，共面 XY 或用户选平面）；复用 `SketchFeature` + `Plane`。
- 参数：深度 `distance`（总深度；可负 = 整体反向）；**`symmetric`（M1 必做）**：勾选后向两侧各拉 `distance/2`。
- 内核：`ExtrudeFeature` 增加 `symmetric` 参数 → `Rebuild` 写入 `ops::ExtrudeSpec::Symmetric`（字段已有）。
- Adapter：`Part::AddExtrude` 或等价 `AppendFeature`；`DocumentService` 同步 ECS。
- 预览：拉伸方向箭头 + 棱柱线框；AccuSnap 地面/面。
- Undo/Redo、XL roundtrip（扩展 XL 节点 `Extrude`，若尚未序列化则 M1 必做）。

**不做（M1）：**

- 复杂约束求解（水平/相切/尺寸驱动）— 仅固定点坐标。
- 非平面轮廓、**拔模角**（见 M5）。
- 多轮廓合并一次 Pad（可 M2 前用多个 Sketch + 布尔并）。

**验收：**

- [ ] 矩形草图拉 20mm → Solid；改深度 Regenerate 高度变。
- [ ] **对称拉伸**：总深 20mm + symmetric → 草图面两侧各 10mm。
- [ ] 带孔轮廓（Inner loop）拉伸与 `TestExtrudeHoles` 行为一致。
- [ ] Delete + Undo 恢复；`.xl` 往返（含 `symmetric` 字段）。

### 3.2 M2 — 挤压（Pocket / Extrude Cut）

**目标：** 选中 **目标 Solid** + **草图轮廓** → 沿草图法向切除材料。

**做：**

- 命令 `part.extrude_cut`（`ExtrudeCutTool`）。
- 特征 `ExtrudeCutFeature`：`targetFeatureId` + `sketchFeatureId` + `depth` + `throughAll`（可选）。
- **Rebuild 策略（v1 推荐）：**
  1. 由草图生成 **工具体** `tool = ops::Extrude(sketch, depth)`；
  2. `BooleanFeature(Subtract, target, tool)` 或 `Part::AddBoolean` 内联；
  3. 抑制/丢弃临时 tool 特征（与现有 Boolean 操作体抑制规则一致）。
- 深度：有限深度 + **贯穿全部**（`throughAll`：深度取目标 AABB 沿法向长度 × 1.01）。

**不做（M2）：**

- 到指定面终止（Up to Surface）。
- 开放轮廓开槽（需 2.5D 刀路语义）。

**验收：**

- [ ] 盒子上草图矩形 Cut 10mm 出现腔体；Undo 恢复。
- [ ] `throughAll` 贯穿通孔。
- [ ] 与 M1 Pad 结果做 Cut，布尔 Pipeline 稳定（平面体）。

**依赖：** M1 草图 + [ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md) 平面布尔已绿。

### 3.3 M3 — 扫掠（Sweep v1）

**目标：** **平面闭合截面**（草图）沿 **空间路径**（Wire Body 或折线）扫掠成 Solid。

**做（v1 刻意收窄）：**

- 命令 `part.sweep`（向导：选截面 → 选路径 → 深度/比例预览 → OK）。
- 路径 v1：`BezierCurveFeature` 或 **3D 折线**（≥2 点）；路径 **G0 连续** 即可。
- 截面 v1：**圆形**（半径）或 **矩形**（Sketch 在路径起点 Frenet 标架内）；截面 **刚体沿路径平移**（无法向扭转 / 无 Parallel transport 时可 **固定截面方向** 为 World Up，文档注明）。
- 特征 `SweepFeature`：`profileSketchId` + `pathFeatureId`（Wire）+ 选项 `fixedDirection`。
- 内核 **二选一**（实现前 ADR 0012 小决策）：
  - **A. 网格缝合近似**：扫掠采样 → 三角柱条 → `ValidateBody` 宽松（学习型，快）；
  - **B. 规则扫掠**：直线路径 + 恒定截面 → 棱柱/柱壳解析；曲线路径 → 分片 Extrude + 布尔并（仅折线路径 v1）。

**不做（M3）：**

- 扭转角（Twist）、缩放（Scale）、引导线（Guide rails）。
- 非平面截面、NURBS 截面。

**验收：**

- [ ] 矩形截面 + L 形折线路径 → 合法 Solid 或 Sheet（v1 可 Sheet，M3.1 升级 Solid）。
- [ ] 路径为 Bezier 弧 → 视觉连续；`ValidateBody` 通过或明确降级为 Sheet。

**依赖：** M1 截面；`BezierCurveFeature`；若走 B 需 M2 布尔并稳定。

### 3.4 M4 — 放样（Loft）与融合（Blend）

**产品确认（2026-08-31）：** **融合 = 曲面过渡 Blend**（两 Wire/边链之间过渡面），**不是** 布尔并（Union）。

**目标：**

- **放样：** ≥2 个 **平行或一般位姿** 的闭合截面，之间过渡成 Solid/Shell。
- **融合（Blend）：** **两条边链 / 两条 Wire** 之间生成 **过渡 Sheet**（G0）；G1 为 M4.1 增强。

**做（v1 刻意收窄）：**

- 命令 `part.loft`、`part.blend`。
- 特征 `LoftFeature`（`std::vector<FeatureId> sectionIds` + `solidOrSheet`）、`BlendFeature`（`wireA`, `wireB` + `continuity`）。
- 截面 v1：**2～3 个** 平面 Sketch，法向大致同向；**直纹 Loft**（相邻截面顶点一一对应，线性插值）→ 侧壁为平面四边形 → 缝合为 Solid。
- **融合（Blend）v1：** **两开放 Wire** 之间 **直纹四边形带** → Sheet Body；与 Loft 两截面特例几何相近，但命令/特征独立，便于以后接 G1 约束。

**不做（M4）：**

- 引导线、中心线、闭合点映射优化、G2。
- 任意 NURBS Loft（需 ADR 0011 + Trim/Sew）。
- **布尔并** — 已有 `boolean.union`，不得并入 `part.blend`。

**验收：**

- [ ] 两矩形截面不同高度 Loft → 棱台状 Solid。
- [ ] 两开放 Bezier 边 **Blend（融合）** → Sheet Body，显示无破面。
- [ ] Ribbon/菜单中 **Blend/融合** 与 **Union/并集** 文案不混淆。

**Gate（M4 开工前）：**

| # | 条件 |
|---|------|
| G4a | [NURBS 路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md) **G3** 最小 Trim/Sew **或** 明确 M4 仅直纹多边形缝合 |
| G4b | NURBS 显示 E2E（ADR 0009 N2+）以便预览 |

### 3.5 M5 — 拔模（Draft）

**目标：** Pad / Pocket 侧壁可设 **拔模角** θ，相对草图法向（或指定中立面）外扩或内收。

**做：**

- `ExtrudeFeature` / `ExtrudeCutFeature` 扩展：`draftAngle`（度）、`draftOutward`（bool）。
- `ops::Extrude` 或专用 `ops::ExtrudeWithDraft`：竖直段 + 斜侧壁（平面或分片平面近似）。
- Viewer：Pad/Pocket 属性面板「Draft angle」；预览显示锥台轮廓。

**不做（M5）：**

- 变角拔模、多中立面、自动分模线。

**验收：**

- [ ] 矩形 Pad 深 20mm、拔模 3° → 顶面大于底面（外拔模）；Regenerate 改 θ 生效。
- [ ] Pocket + 拔模 → 腔体侧壁倾斜，布尔 Cut 仍合法。

**依赖：** M1 Pad、M2 Pocket。

## 4. 特征与内核 API（目标形状）

### 4.1 新/扩特征类型

| TypeName | 类 | 产出 | 主要参数 |
|----------|-----|------|----------|
| `Sketch` | 已有 | — | 轮廓实体 |
| `Extrude` | 已有 | Solid | sketchId, distance, **symmetric** (M1), draftAngle (M5) |
| `ExtrudeCut` | **新** | Solid（修改 target） | targetId, sketchId, depth, throughAll, draftAngle (M5) |
| `Sweep` | **新** | Solid/Sheet | profileSketchId, pathWireId, fixedDirection |
| `Loft` | **新** | Solid/Sheet | sectionIds[], ruled |
| `Blend` | **新** | Sheet（曲面过渡） | wireAId, wireBId, continuity |

均实现 `IFeature::Rebuild` / `CollectParameters`；`ToPrimitiveSpec` 返回 `nullopt`（与 Boolean/Extrude 相同，Copy 走 `DuplicateBody`）。

### 4.2 `Part` 便捷 API（占位）

```cpp
Body* AddExtrudePad(feat::FeatureId sketch, double depth, std::string name = "Pad");
Body* AddExtrudeCut(feat::FeatureId target, feat::FeatureId sketch,
                    double depth, bool throughAll, std::string name = "Pocket");
Body* AddSweep(feat::FeatureId profileSketch, feat::FeatureId pathWire,
               const SweepOptions& opt, std::string name = "Sweep");
Body* AddLoft(std::span<const feat::FeatureId> sections,
              const LoftOptions& opt, std::string name = "Loft");
Body* AddBlend(feat::FeatureId wireA, feat::FeatureId wireB,
               BlendContinuity c, std::string name = "Blend");
```

### 4.3 新 ops（`brep_core` / `brep_feat`）

| 算子 | 说明 | 分期 |
|------|------|------|
| `ops::Extrude` | 已有；M1 接 Symmetric | M1 |
| `ops::ExtrudeWithDraft` | 侧壁锥角 | M5 |
| `ops::ExtrudeCut` | 包装 Extrude + 布尔减，或 Imprint+Select 专路径 | M2 |
| `ops::SweepRuled` | 截面沿折线/采样路径扫掠 | M3 |
| `ops::LoftRuled` | 直纹 Loft 多截面 | M4 |
| `ops::BlendRuled` | 两 Wire 间 ruled surface | M4 |

复杂 NURBS 版本预留 `ops::SweepNurbs` / `LoftNurbs`（ADR 0011 后）。

---

## 5. Viewer 命令与 UI

### 5.1 命令一览

| id | 工具类 | Ribbon（建议） | 快捷键建议 |
|----|--------|----------------|------------|
| `part.extrude_pad` | `ExtrudePadTool` | Modeling → Extrude | 无（`Ctrl+E` 预留） |
| `part.extrude_cut` | `ExtrudeCutTool` | Modeling → Extrude Cut | — |
| `part.sweep` | `SweepTool` | Modeling → Sweep | — |
| `part.loft` | `LoftTool` | Modeling → Loft | — |
| `part.blend` | `BlendTool` | Modeling → Blend | — |
| `part.create_sketch` | `CreateSketchTool` | Modeling → Sketch | — |

交互模式对齐现有 `CreateBoxTool` / `CreateBezierTool`：`ITool`、AccuSnap、`SetPreviewEdges`、`CommandKind::Interactive`。

### 5.2 ExtrudePadTool 状态机（M1 示例）

```text
[PickPlaneOrUseDefault] → [PickProfilePoints…] → [PickDepthOrNumeric] → commit
         │ ESC 取消全程；Backspace 撤点
```

| 步骤 | 用户动作 | 预览 |
|------|----------|------|
| 0 | 选平面（v1 可默认 XZ 地面） | 平面栅格 |
| 1…n | 左键加顶点，闭合（近距吸附首点） | 闭合多边形 + 法向箭头 |
| n+1 | 拖动或输入深度；可勾选 **Symmetric** | 棱柱线框（对称时两侧） |

**ExtrudeCutTool：** 先选目标体（`BodyRef`）→ 进入与 Pad 相同草图/深度 → 提交 Cut。

**SweepTool / LoftTool：** 向导对话框 + 视口拾取；未完成前可用分步 `ITool`。

### 5.3 属性面板

- Pad/Pocket：深度、**Symmetric（M1）**、throughAll（Cut）、**Draft angle（M5）**。
- Sweep：路径特征名、固定方向 checkbox。
- Loft：截面列表顺序、 ruled 只读 true（v1）。
- Blend（融合）：Wire A/B、连续性（G0 只读 v1）。

### 5.4 i18n 源文（英文 → 中文）

| 英文源文 | 中文 |
|----------|------|
| Extrude (Pad) | 拉伸（凸台） |
| Extrude Cut (Pocket) | 挤压（切除） |
| Symmetric | 对称拉伸 |
| Draft angle | 拔模角 |
| Loft | 放样 |
| Sweep | 扫掠 |
| Blend | 融合（曲面过渡） |
| Boolean Union | 布尔并集（**不用「融合」**） |
| Pick profile point | 拾取轮廓点 |
| Specify extrude depth | 指定拉伸深度（总深度） |
| Select target body | 选择目标实体 |

写入 `RibbonSetup` / 各 Tool 的 `QCoreApplication::translate` + `xcad_zh_CN.ts`。

---

## 6. 数据流

```text
Viewer ITool
    │ pick / preview (EdgeMesh)
    ▼
CommandsBridge → DocumentService / ISceneService
    │ AppendFeature + Regenerate
    ▼
Part::FeatureTree
    │ ExtrudeFeature / SweepFeature / …
    ▼
ops::*  →  Model (B-Rep)
    │ SyncPartBodies
    ▼
ECS BodyRef + RenderCache.force_rebuild
```

布尔 Cut（M2）路径：

```text
ExtrudeCutFeature::Rebuild
    → toolBody = ops::Extrude(sketch)
    → evaluator.Subtract(targetBody, toolBody)
    → replace target BodyGuid
```

---

## 7. 持久化（XL）

M1 起扩展 XL schema（与 `BoxFeature` / `ExtrudeFeature` 序列化对齐）：

| 节点 | 字段 |
|------|------|
| `SketchFeature` | id, plane(origin, normal, xAxis), entities[], constraints[]（v1 可空） |
| `ExtrudeFeature` | id, sketchRef, distanceParam, **symmetric**, bodyGuid |
| `ExtrudeCutFeature` | id, targetRef, sketchRef, depth, throughAll, **draftAngle** (M5), bodyGuid |
| `SweepFeature` | id, profileRef, pathRef, options, bodyGuid |
| `LoftFeature` | id, sectionRefs[], ruled, bodyGuid |
| `BlendFeature` | id, wireARef, wireBRef, continuity, bodyGuid |

版本号 bump；`examples/xl_roundtrip` 与 `TestDocumentScope` 扩展。

---

## 8. 测试策略

| 层级 | 内容 |
|------|------|
| 内核 GTest | `ops::Extrude` 扩展、`ExtrudeCut` 减材、`SweepRuled`/`LoftRuled` 合法体、`ValidateBody` |
| 特征 | `ExtrudeFeature`/`ExtrudeCutFeature` Rebuild、抑制、失败恢复 |
| Viewer | `TestSceneAdapter` Regenerate 后 mesh 更新；命令 Undo 链 |
| 回归 | 现有布尔/拉伸孔/Copy-Move 不回归 |

---

## 9. 风险与决策点

| 风险 | 缓解 |
|------|------|
| 草图求解器范围膨胀 | M1 仅折线/矩形；约束进 M5 |
| Cut = Extrude + 布尔慢/不稳定 | 平面 Cut 专 fast path；失败提示换 throughAll |
| Sweep/Loft v1 非 NURBS 精度 | 文档标注「直纹/折线近似」；ADR 0011 接 NURBS 版 |
| 「融合」与「布尔并」混淆 | **已确认** Blend=曲面过渡；UI 并集只用 Union/并集；旧文档「融合体」→「并集体」 |
| 结果体 Move 失败 | 保持 G6 现状；Placement 另开任务 |

**开工前小 ADR（建议 ADR 0012）决策：**

1. M3 Sweep v1 走 **网格缝合** 还是 **分段 Extrude+Union**？
2. M4 是否在无 Trim/Sew 时允许 **仅 Sheet** 产出？
3. `ExtrudeCutFeature` 是否 **内嵌** Boolean 还是 **子特征** 可见于树？

---

## 10. 非目标（全文）

- 旋转凸台（Revolve）、肋、抽壳、阵列（可引用其他 spec）。
- 完整 NURBS 曲面求交、G2 Blend、引导线 Loft。
- 将 `boolean.union` **中文** 称作「融合」（与 Blend 冲突）。
- OCCT 依赖（除非另开 ADR 修订 0005）。
- 工程图、2D 投图。

（**拔模** 在 **M5** 范围内，不属于「全文非目标」。）

---

## 11. 里程碑总览

| 里程碑 | 交付物 | 用户可见 |
|--------|--------|----------|
| **M1** | 草图 + Pad + **对称拉伸** + XL | Ribbon「拉伸」、Symmetric 勾选 |
| **M2** | ExtrudeCut | 「挤压/切除」、通孔 |
| **M3** | Sweep v1 | 截面+路径扫掠 |
| **M4** | Loft + **Blend（融合/曲面过渡）** | 多截面放样、两 Wire 融合 |
| **M5** | Draft 拔模 | Pad/Pocket 拔模角 |

---

## 12. 文档索引

| 文档 | 关系 |
|------|------|
| [2026-08-24-nurbs-modeling-ui-roadmap.md](./2026-08-24-nurbs-modeling-ui-roadmap.md) | M4 远期 NURBS 版 Loft/Sweep |
| [2026-08-27-bezier-curve-command-design.md](./2026-08-27-bezier-curve-command-design.md) | Sweep 路径输入 |
| [2026-08-27-move-command-interaction.md](./2026-08-27-move-command-interaction.md) | 结果体 Move/Copy |
| [2026-08-24-boolean-general-pipeline-only-design.md](./2026-08-24-boolean-general-pipeline-only-design.md) | M2 Cut 依赖 |
| [ADR 0011 占位](../../architecture/layering.md) | NURBS 建模正式立项 |

---

## 13. 建议下一步

1. ~~评审「挤压 = Pocket」「融合 = Blend」~~ **已确认（2026-08-31）**。
2. M1 立项：Viewer 草图 + `ExtrudePadTool` + **Symmetric**；XL 序列化。
3. M1 完成后 M2 Pocket；M5 拔模在 M2 之后排期。
4. M3/M4 前写 **ADR 0012**（Sweep/Loft v1 实现路径 + Cut 特征形态）。
