# 棱柱 × 球 布尔测试矩阵

**日期：** 2026-09-18  
**状态：** Phase A + B + C 已落地；`GAP-lune-stitch` / `GAP-edge-pair` 已关  
**实现入口：** [`tests/kernel/TestBooleanSizeSweep.cpp`](../../../tests/kernel/TestBooleanSizeSweep.cpp)  
**关联：** [ADR 0008 仅通用 Pipeline](../../architecture/adr/0008-boolean-general-pipeline-only.md)、[2026-08-10 球布尔设计](./2026-08-10-analytic-sphere-brep-boolean-design.md)、[2026-08-24 通用路径](./2026-08-24-boolean-general-pipeline-only-design.md)

**Goal：** 用一张可扩展的姿态 × 边数 × 操作矩阵驱动内核迭代。失败用例**留在表里**（用 `Expect*` 或 `KNOWN_GAP` 标明），禁止「先删掉再宣称全绿」。

---

## 1. 为什么现有用例不够

生成器已收成 `ProfileKind::{Rect,Regular}` + `Sides` + `MakeRegularPrism`。Phase A 覆盖正 3/4/5/6 边形与正交矩形；七棱+、不规则剖面仍属缺口。

Phase A/B/C 的 CSG 四条（除 `EmptyCsg`）现已全开。`VertexNick` 靠分类：lune patch 用面内采样、两顶点开弧 sliver 不用印记圆心（球心在棱上会退化成 On/Out）。`EdgeBite` 开弧配对已绿。

---

## 2. 现状盘点（2026-09-18）

### 2.1 已实现姿态（Phase A + 遗留矩形/六棱/盒）

每个姿态默认跑 4 条：`∪`、`∩`、`棱−球`、`球−棱`，`AllowOperandSwap=false`。  
`棱−球` 额外要求球面接触面三角数 > 0。`Expect*=false` 必须带 `SkipReason`（`EmptyCsg` 或 `KNOWN_GAP:id`）。

| ID | 族 | 姿态类 | 四条操作 | 备注 |
|----|----|--------|----------|------|
| RectThroughBaseline | 4 正交 | 贯穿（居中） | 全开 | 基准 |
| RectThroughThin / Taller / Large | 4 正交 | 贯穿（高/径变） | 全开 | |
| RectThroughOffset | 4 正交 | 贯穿 + 偏出两壁 | 全开 | 4 个印记圆 |
| RectBuriedSphere | 4 正交 | 球完全在板内 | 球−棱 关 | `EmptyCsg` |
| RectThroughThickTangent | 4 正交 | 内切四壁 | 球−棱 关 | `EmptyCsg` |
| RectTop/BottomCapBite | 4 正交 | 顶/底帽咬 | 全开 | |
| RectSideBite / CornerBite | 4 正交 | 侧面 / 三面角咬 | 全开 | |
| RectEdgeBite | 4 正交 | 竖棱外双面开弧 | 全开 | 原 GAP-edge-pair 已关 |
| RectVertexNick | 4 正交 | 竖棱 lune | 全开 | 原 GAP-lune-stitch 已关 |
| HexUntitledLike / HexScaleHalf | 6 正 | untitled 比例 | 全开 | **正**六棱，不是桌面不规则六棱 |
| HexTopNick | 6 正 | 顶面大圆 nick | 全开 | |
| HexVertexNick | 6 正 | 竖棱 lune | 全开 | 原 GAP-lune-stitch 已关 |
| SphereContainsHex | 6 正 | 球包棱柱 | 棱−球 关 | `EmptyCsg` |
| Tri/Quad/Pent/Hex Regular ThroughCentered | 3/4/5/6 正 | 贯穿居中 | 全开 | Phase A |
| Tri/Quad/Pent/Hex Regular TopBite | 3/4/5/6 正 | 顶帽咬 | 全开 | Phase A |
| Tri/Quad/Pent/Hex Regular SideBite | 3/4/5/6 正 | 侧面咬 | 全开 | Phase A |
| Quad/Pent/Hex Regular EdgeBite | 4/5/6 正 | 竖棱外 | 全开 | |
| TriRegularEdgeBite | 3 正 | 竖棱外双面开弧 | 全开 | 原 GAP-edge-pair 已关 |
| Tri/Quad/Pent Regular VertexNick | 3/4/5 正 | 竖棱 lune | 全开 | 原 GAP-lune-stitch 已关 |
| Tri/Quad/Pent Regular Contains | 3/4/5 正 | 球包棱柱 | 棱−球 关 | `EmptyCsg` |
| Tri/Pent Regular ThroughThin/Tall/R04/R07 | 3/5 正 | Phase B 贯穿尺寸 | 全开 | |
| Tri/Pent Regular ThroughR12 | 3/5 正 | r=1.2R 包住棱柱 | 棱−球 关 | `EmptyCsg`；并集靠球面投影采样 |
| Tri/Pent Regular ThroughOffsetSide/Vertex | 3/5 正 | 贯穿偏侧 / 偏顶点 | 全开 | 偏顶点未落到竖棱上 |
| Tri/Pent Regular TopBiteThin/Tall/Large | 3/5 正 | Phase B 顶咬尺寸 | 全开 | |
| Tri/Pent Regular SideBiteThin/Tall/Large | 3/5 正 | Phase B 侧咬尺寸 | 全开 | |
| PentRegularEdgeBiteThin/Tall/Large | 5 正 | Phase B 棱外尺寸 | 全开 | n=3 EdgeBite 不加密 |
| HexIrregularUntitled | 6 不规则 | untitled 桌面球 | 全开 | 烘焙自 Guid `11de7787` / `0a680dea` |
| QuadTrapezoid Through/TopBite/SideBite | 4 梯形 | Phase C | 全开 | |
| PentIrregular Through/TopBite/SideBite | 5 不规则 | Phase C | 全开 | |
| OctantUnit/Half/Double | 盒 | 球心在角、r=盒边 | 全开 | 无 swap |
| BoxThrough* / CornerClip / FaceBite | 盒 | 贯穿 / 角裁 / 面咬 | 全开 | |

Lock：`BooleanLock.RectPadSphereBothOrdersNoSwap` / `HexPadSphereBothOrdersNoSwap`（指纹，不是矩阵）。

### 2.2 明确缺口

| 缺口 | 说明 |
|------|------|
| **边数** | 无 7+；四棱无菱形、斜平行四边形（正四边形已有） |
| **非正多边形** | 矩阵已有梯形 / 不规则五棱 / untitled 烘焙六棱；Guid 锁仍在独立 untitled 测试 |
| **lune 缝合** | 球心在竖棱上，两段大圆弧共用两端点 → patch 缺伴侣（3/4/5/6 与正交矩形均复现） |
| **邻面开弧配对** | 竖棱外、两开弧端点不共用时，直角/锐二面角的 ∩ 与 棱−球 仍缺伴侣 |
| **凹多边形** | `TestPlanarBoolean` 里 L 形仍有 DISABLED |
| **圆柱 / 二次结果再布尔** | 不在本矩阵（远期） |

---

## 3. 用例怎么写（契约）

### 3.1 一条用例是什么

一条 = **棱柱规格 + 球规格 + 姿态类 + 每条操作的期望**。

```text
Case = {
  Name,                    // PascalCase，含边数与姿态，如 TriThroughBaseline
  Sides,                   // 3, 4, 5, 6, 7, 8, …
  Profile,                 // Regular(R) | Rectangle(W,D) | FromFile | 显式顶点
  Height, Plane,           // 默认 XzYUp，与现 Extrude 一致
  SphereCenter, SphereRadius,
  PoseClass,               // 见 §4
  Expect[Union, Intersect, PrismMinusSphere, SphereMinusPrism],
  GapId                    // 可选，指向 §7
}
```

实现上把现在的 `PadShape::{Rect,Hex}` 收成 `Sides` + `ProfileKind`，正 n 边形用同一套生成：

```text
p_i = R * (cos(2π i / n), sin(2π i / n))   // XZ 剖面，挤出 +Y
```

矩形继续走轴对齐剖面（与正四边形不同：正四边形侧面是 45° 旋转的）。

### 3.2 每条操作断言（最低）

| 检查 | 何时 | 失败含义 |
|------|------|----------|
| `Evaluate` 返回 `Ok` 且有 `OutputBody` | 期望非空结果 | Pipeline 某阶段失败 |
| `ValidateBody` 无 Error | 同上 | 缺伴侣、径向次数 ≠ 2、邻接顶点断开 |
| 球面三角数 > 0 | 仅 `棱−球` 且结果里还有球面 | 凹腔面在但没网格（白洞） |
| 空结果被允许 | `Expect*=false` | 必须在表里写清「空是合法 CSG」还是「KNOWN_GAP」 |

禁止用「删用例」代替 `Expect*=false`。`false` 必须带原因：`EmptyCsg` 或 `KNOWN_GAP:<id>`。

### 3.3 操作数顺序

矩阵默认 **`AllowOperandSwap=false`**（与 Viewer 点选顺序一致）。  
`棱 ∪ 球` 与 `球 ∪ 棱` 对并/交应同胚；差集两条都要跑。盒八分之一球已证明「只测默认 swap」会漏 split 选楔 bug。

### 3.4 命名

```text
{Sides}{Profile}{Pose}{Qualifier}

TriRegularThroughBaseline
QuadRectThroughOffset
PentRegularVertexNick
HexIrregularUntitled
HeptRegularTopNick
```

---

## 4. 姿态分类（与边数正交）

边数只改「有几张侧面、法向夹角」；姿态决定球和棱柱怎么相交。实现时 **每个姿态先在 n=3,4,5,6 上各落 1 条**，再加密尺寸。

| PoseClass | 几何意图 | 交线形态（典型） | 内核压力点 |
|-----------|----------|------------------|------------|
| `ThroughCentered` | 球心在柱内，上下底都切穿 | 2 个闭圆（顶+底） | remainder 带；两洞赤道采样 |
| `ThroughOffset` | 贯穿且偏出 1～2 张侧面 | 闭圆 ≥ 3 | 多洞 remainder 采样落在帽上 |
| `ThroughThin` / `ThroughTall` | 只改高度/半径比 | 同上 | 数值尺度 |
| `Buried` | 球 ⊆ 棱柱，不穿壁 | 无交或切点 | 球−棱 = 空（`EmptyCsg`） |
| `Contains` | 棱柱 ⊆ 球 | 无交或切点 | 棱−球 = 空 |
| `TopBite` / `BottomBite` | 球心在底面外，切一个底 | 1 个闭圆 | 小帽极点 vs leftover |
| `SideBite` | 球心在一张侧面外 | 1 个开弧或闭圆 | 单面 split |
| `EdgeBite` | 球心在一条**竖棱外**（不在棱上） | 2 个开弧，端点不共用 | 双面 patch |
| `VertexNick` | 球心 **在** 竖棱上 | 2 段大圆弧，**共用两端点（lune）** | 缝合 `SameGeometricCurve` |
| `TopVertexNick` | 球心在顶面多边形顶点 | 底面圆 + 两侧开弧 | 三面汇 | 
| `GreatCircleCap` | 球心在底面内、平面过球心 | 1 个大圆 | 采样不能 `continue` 掉同心圆 |
| `TangentWall` | 半径 = 到侧面距离 | 退化点/零半径圆 | 切点面心 On，需顶点投票 |
| `IrregularFile` | 磁盘上的真实剖面 | 混合 | 回归 untitled |

`EdgeBite` 与 `VertexNick` 必须分开：前者两端点一般不重合，现有开弧 imprint 较能过；后者是已知 lune 洞。

---

## 5. 推荐矩阵（第一批要落地的）

原则：少而全 —— **每个 (边数 × 姿态大类) 至少 1 条**，尺寸扫描放第二批。

### 5.1 Phase A — 边数铺开（正 n 边形，R=2，H=1.2，球 r=0.7 除非另写）

球心坐标用棱柱局部架：原点在剖面形心，+Y 为拉伸。

| 边数 | ThroughCentered | TopBite | SideBite | EdgeBite | VertexNick | Contains |
|------|-----------------|---------|----------|----------|------------|----------|
| 3 | 绿 | 绿 | 绿 | 绿 | 绿 | 绿（EmptyCsg） |
| 4 正（旋转 45°） | 绿 | 绿 | 绿 | 绿 | 绿 | 绿（EmptyCsg） |
| 4 正交矩形 | 已有绿 | 已有绿 | 已有绿 | 绿 | 绿 | Buried/Tangent = EmptyCsg |
| 5 | 绿 | 绿 | 绿 | 绿 | 绿 | 绿（EmptyCsg） |
| 6 正 | HexRegularThroughCentered 绿 | HexRegularTopBite + HexTopNick 绿 | HexRegularSideBite 绿 | HexRegularEdgeBite 绿 | HexVertexNick 绿 | SphereContainsHex |
| 8 | 选做（逼近圆柱） | 选做 | — | — | — | — |

Phase A 已抄进 `INSTANTIATE_TEST_SUITE_P`（约 +24 条新用例）。红格一律留在表里并挂 GapId。

### 5.2 Phase B — 尺寸与偏置（先在 n=3 与 n=5 上扫，避免只扫矩形）

每个已绿的 PoseClass 再变（H=1.2，R=2 外接圆）。**不**在 VertexNick / n=3 EdgeBite 上加密。

| 族 | 变体 | 用例名 |
|----|------|--------|
| Through n=3 | 0.4H / 2H / 0.4R / 0.7R / 1.2R / 偏侧 / 偏顶点 | `TriRegularThroughThin` `Tall` `R04` `R07` `R12` `OffsetSide` `OffsetVertex` |
| Through n=5 | 同上 | `PentRegularThrough*` |
| TopBite n=3,5 | 0.4H / 2H / 0.4R | `*TopBiteThin` `Tall` `Large` |
| SideBite n=3,5 | 0.4H / 2H / 0.4R | `*SideBiteThin` `Tall` `Large` |
| EdgeBite n=5 | 0.4H / 2H / 0.4R | `PentRegularEdgeBiteThin` `Tall` `Large` |

`ThroughR12`（r=2.4）包住棱柱顶点，自动标 `EmptyCsg`（棱−球空）。+29 条，**四条操作全绿**（含 `ThroughR12` 并集：整球采样点投影到球面，不再用两极+缝的体心）。只对 Phase A 已全绿姿态加密。不在 VertexNick / n=3 EdgeBite 上爆炸。

### 5.3 Phase C — 不规则与文件

剖面走 `ProfileKind::Explicit`（`ProfileUv`），不依赖桌面文件也能跑。Guid 锁仍在 `UntitledSphereExtrudeBoolean`（Pad `11de7787…` / sphere `0a680dea…`）。

| ID | 来源 | 姿态 | 目的 |
|----|------|------|------|
| HexIrregularUntitled | 从 `untitled.xl` 烘焙的 6 顶点 + 原球 | 桌面偏置大切 | 非正六棱 |
| QuadTrapezoidThrough / TopBite / SideBite | 显式梯形 `(0,0)(3,0)(2.2,1.8)(0.6,1.8)` | 贯穿 / 顶咬 / 侧咬 | 非平行四边形 |
| PentIrregularThrough / TopBite / SideBite | 正五边形拉长一顶点、缩短另一顶点 | 同上 | 边长不等 |

### 5.4 盒族

盒 = 正交四棱柱特例，保留现有 Octant / Through / FaceBite，不与正四边形合并（角点在坐标原点 vs 旋转 45°）。

---

## 6. 期望结果怎么填

| 姿态 | ∪ | ∩ | 棱−球 | 球−棱 |
|------|---|---|--------|--------|
| Through*（非相切） | 体 | 体，有球面 | 体，有孔+球面凹腔 | 体，两帽或一带 |
| Buried | = 棱 | = 球 | 内腔（有球面） | **空** `EmptyCsg` |
| Contains | = 球 | = 棱 | **空** `EmptyCsg` | 球壳减棱 |
| *Bite / *Nick | 体 | 体 | 体+凹腔网格 | 体（缺一块） |
| TangentWall（球 ⊆ 棱） | = 棱 | = 球 | 内腔或切接触 | **空** `EmptyCsg` |
| VertexNick（lune） | 体 | 体 | 体+凹腔网格 | 体（缺一块） |

空结果必须 `Evaluate` 失败或明确「无 OutputBody」且测试标 `EmptyCsg`，不能和 `ValidateBody` 失败混成一谈。

---

## 7. 内核缺口台账（KNOWN_GAP）

测试文件里用注释 + `Expect*=false` 挂 id。本文是唯一台账，修掉一条就改状态并打开断言。

| GapId | 症状 | 触发几何 | 现状 | 拟改位置 |
|-------|------|----------|------|----------|
| `GAP-lune-stitch` | patch/split 缺伴侣，径向次数 1 | 球心在竖棱，两开弧共两端点 | **已关**（2026-09-18）：patch Union 用面内采样；两顶点开弧 sliver 用 `FaceSamplePoint` | `Pipeline` 分类 |
| `GAP-edge-pair` | 两邻面开弧印记配对失败，径向次数 1 | 球心在竖棱外，两开弧端点不共用；直角/锐二面角 | **已关**（2026-09-18）：开弧 `SameGeometricCurve` + `MergeCoedgeOntoEdge` | `BooleanBuilder` |
| `GAP-concave-imprint` | 凹 L 面不劈 | L 形挤出 | `TestPlanarBoolean` DISABLED | Imprint 一般平面 |
| `GAP-zero-circle` | 切点当闭圆 | 半径 ≈ 到墙距离 | 矩形相切已用顶点投票绕过 | Intersect 层丢弃 r≤eps 的圆 |

新失败必须先登记再允许 `Expect*=false`。禁止只写「先不加这个 case」。

---

## 8. 实现步骤（文档落地到代码）

1. **改生成器**（不先加 30 条）：`Sides` + `MakeRegularPrism(n, R, H)`，`Rect` 保留为正交特例。
2. **Phase A 表格抄进 `INSTANTIATE_TEST_SUITE_P`**，新姿态默认四条全开；若红，登记 GapId 再关对应操作。
3. **跑** `brep_test_boolean_size_sweep`，按 Gap 迭代内核（与本次尺寸扫描同一循环）。
4. **Lock 不加 n 变体**（指纹太脆）；lock 仍只锁 1 个矩形 + 1 个正六棱基准。
5. **Phase B** 已落地（n=3/5 尺寸+偏置，+29）。
5b. **Phase C** 已落地（不规则 + untitled 烘焙，+7；`brep_test_boolean_size_sweep` 84）。
6. 本文件 §2 / §7 随代码更新；新增用例先改表再改 cpp。

---

## 9. 不做（本矩阵范围外）

- Viewer 拾取 / 操作数顺序 UX（见 [两体交互](./2026-09-17-boolean-two-body-interaction.md)）。
- 棱柱 × 棱柱、球 × 球（可另开矩阵，复用 PoseClass 名字）。
- NURBS 体、网格对比金样。
- 用随机顶点爆炸组合（不可复现、难对应 Gap）。

---

## 10. 验收

- [x] `MakeRegularPrism` 支持 n=3…8，矩形仍走正交路径。
- [x] Phase A 表中每一格有对应用例名；绿或挂 `KNOWN_GAP`。
- [x] `GAP-lune-stitch` / `GAP-edge-pair` 已关；VertexNick 与 EdgeBite 四条操作全开。
- [x] `ctest -R boolean_size_sweep` 与 `brep_test_extrude_pad_boolean` lock 在每次迭代后都跑。
- [x] 不再出现「红了就从 `Values(...)` 里删 case」。
- [x] Phase B：n=3/5 已绿姿态做高度 / 半径 / 偏置加密（+29）；`ThroughR12` 并集经球面采样投影修复。
- [x] Phase C：`HexIrregularUntitled`（untitled 烘焙剖面）+ 梯形 / 不规则五棱 Through/TopBite/SideBite 全绿。
