# NURBS 特征树与自由曲面建模 UI — 远期规格（占位）

**日期**：2026-08-24  
**状态**：远期占位（**未排期、未接受实现承诺**）  
**前置**：[ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)（Display）、[NURBS 综合方案](./2026-08-24-nurbs-unified-strategy.md)、[NURBS 显示设计](./2026-08-24-nurbs-display-viewer-design.md)、[NURBS 显示实施计划](../plans/2026-08-24-nurbs-display-implementation.md)  
**关联**：`IFeature` / `FeatureTree`、`Part::Regenerate`、Viewer 命令桥（`CommandsBridge`）

> 本文档描述 **「能建、能改、能再生」** 的 NURBS 建模能力，与 ADR 0009 的 **Display-only** 分离。  
> 开工前需：**修订 ADR 0009 或新增 ADR 0011**（0010 为内核库拆分），并确认前置里程碑（至少 NURBS 显示 N2+）已达标。分层落点见 [layering.md](../../architecture/layering.md) §7。

---

## 1. 为什么要单独成文

| 层级 | ADR 0009（近期） | 本文档（远期） |
|------|------------------|----------------|
| 内核 | `NurbsCurve/Surface` 求值 + tessellation | Trim / Sew / Loft / Sweep **拓扑构建** |
| 特征 | — | `Nurbs*Feature`、`LoftFeature` 等 |
| Viewer | 显示 mesh / 等参线 | Studio Spline、CV 拖拽、曲率分析 |
| 持久化 | 可选（几何进 Model 即可） | XL 节点、参数、再生语义 |

避免把「画 NURBS」与「用 NURBS 建模」绑在同一 ADR，导致范围膨胀（见 [2026-08-10 布尔设计](./2026-08-10-analytic-sphere-brep-boolean-design.md) 风险表）。

---

## 2. 前置条件（Gate）

在启动 **P1（特征树）** 之前，建议满足：

| # | 条件 | 负责文档 |
|---|------|----------|
| G1 | NURBS 曲线/曲面 **显示** E2E（uniform 或 adaptive tessellation） | [NURBS 显示计划](../plans/2026-08-24-nurbs-display-implementation.md) N2+ |
| G2 | `NurbsCurve` / `NurbsSurface` 在 `Model` 池内可创建、可 `ValidateBody`（Sheet/Solid） | 同上 N1/N4 |
| G3 | （若要做 Solid 造型）NURBS **Trim + Sew** 或等价「面→壳→体」路径有最小实现 | 待专 ADR |
| G4 | （若要与布尔联动）通用布尔 Pipeline 对 **解析面** 稳定；NURBS 布尔另开 ADR | [ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md) |

**G3/G4 可降级**：P1 仅支持 **Sheet Body（单 NURBS 面）** 或 **静态导入面**，不做 Solid 缝合。

---

## 3. 分期愿景（占位）

### P0 — 静态 NURBS 几何入 Model（无 Feature）

**目标**：内核/API 能挂 NURBS 面/线，Viewer 能显示；**不进 FeatureTree**。

- 手工或测试代码 `MakeNurbsSurface` + `MakeFace` + `MakeBody(BodyType::Sheet)`
- 验收：与 ADR 0009 显示里程碑重叠，**不重复开发 UI**

### P1 — 最小 NURBS 特征树

**目标**：历史树里出现可再生的 NURBS 节点；参数变更触发 `Rebuild`。

| 特征（占位名） | 类型 | Rebuild 产出 | 参数示例 |
|----------------|------|--------------|----------|
| `NurbsCurveFeature` | 线框/参考线 | Wire Body 或 ref 曲线 | 控制点表、阶数、节点 |
| `NurbsSurfaceFeature` | 单张曲面 | Sheet Body | 控制网、U/V 阶数、节点 |
| `ImportNurbsFeature`（可选） | 外部导入 | Sheet/ Solid | 文件路径 / 内嵌 blob |

**内核契约**（对齐现有 `BoxFeature`）：

```cpp
class NurbsSurfaceFeature final : public IFeature {
  // Id, TypeName, Status, Suppressed, BodyGuid ...
  bool Rebuild(Part& part, param::ParameterStore& params) override;
  void CollectParameters(param::ParameterStore& store) override;
  // XL 序列化（feat/io 扩展）
};
```

**非目标（P1）**：视口拖 CV、Loft、与 Boolean 组合。

### P2 — 自由曲面 UI（交互编辑）

**目标**：对标 CAD **轻量** Studio Spline / 控制点编辑，仍限于 **单曲线/单曲面**。

| UI 能力 | 说明 | CAD 参照 |
|---------|------|----------|
| 插入 NURBS 曲线 | 三次夹紧有理 B 样条，CV ≥ 4 | [有理 B 样条曲线需求](./2026-09-24-rational-bspline-curve-requirements.md)；AutoCAD SPLINE (CV) |
| 插入 NURBS 曲面 | 4×4 控制网 bilinear → bicubic 种子 | NX Studio Surface 简化版 |
| 显示控制网 | Toggle（Display 层已有占位） | MicroStation Surface Polygon |
| 视口拖 CV | 拾取 CV → 拖动 → `MarkDirty` → Regenerate | AutoCAD 3DEDITBAR |
| 属性面板 | 阶数只读或受限编辑、CV 坐标表 | SolidWorks 草图 spline 简化 |

**Viewer 组件（占位）**：

- `NurbsEditTool` / `ControlVertexPickTool`（commands/tools/）
- `PropertyPanel` 扩展：NURBS 特征属性
- `ViewManager`：控制网 / 等参线 layer 开关

**非目标（P2）**：G1/G2 连续性约束 UI、曲面匹配、曲率梳。

### P3 — 造型特征（Loft / Sweep / Trim）

**目标**：多条曲线/多张面组合成复杂形状；进入 **Solid** 域。

| 特征（占位） | 依赖 |
|--------------|------|
| `LoftFeature` | 多条 `NurbsCurveFeature` 或截面 |
| `SweepFeature` | 路径曲线 + 截面 |
| `TrimSurfaceFeature` | NURBS 求交 + Imprint（内核） |
| `SewFeature` | 多 Sheet → Solid Shell |

**UI**：向导式命令（选截面 → 选路径 → 预览 → OK）。

**前置**：NURBS 求交 ADR、Trim/Sew 内核；否则 P3 仅文档占位。

### P4 — 分析与质量（可选）

| 能力 | 说明 |
|------|------|
| 曲率梳 / 斑马线 | 需要密集 tessellation + 曲率 Eval |
| G0/G1/G2 检查 | 边/面邻接处导数 |
| 与 BooleanFeature 组合 | NURBS Solid 参与 CSG |

---

## 4. 特征树集成要点（占位）

### 4.1 与现有 `FeatureTree` 关系

- 新特征实现 `IFeature`，经 `FeatureTree::Append` 加入，顺序再生。
- `Regenerator` / `Part::Regenerate` 扩展：识别 `TypeName()` `"NurbsSurface"` 等。
- 抑制/撤销/rollback：复用 `FeatureHistory`；NURBS 特征与 `BooleanFeature` 操作体抑制规则一致。

### 4.2 参数化

- 控制点坐标可进 `ParameterStore`（与 Box 的 L/W/H 同级），便于驱动与 XL 存参。
- 阶数/节点向量 **P1 建议固定模板**（如 cubic clamped），避免参数爆炸。

### 4.3 持久化（XL）

- 新节点类型：`NurbsSurfaceFeature`、`NurbsCurveFeature`
- 字段占位：feature id、body guid、degree、knots、control points/weights、suppressed
- 版本号：与现有 XL roundtrip 测试扩展

### 4.4 Viewer / ECS

- `BodyRef{Guid}` 不变；NURBS Body 与 Box 相同同步路径（`SceneAdapter` → tessellation）
- 选中 NURBS 特征时属性面板只读显示 `TypeName` + 参数（P1）；P2 可编辑 CV

---

## 5. 自由曲面 UI 信息架构（占位）

```text
菜单 / 工具栏
  ├─ 插入 → NURBS 曲线
  ├─ 插入 → NURBS 曲面
  └─ 造型 → Loft / Sweep（P3，灰化直至 Gate）

视口
  ├─ Shaded + Edges（默认）
  ├─ 控制网（Debug / 编辑模式）
  └─ 等参线（Wireframe 辅助）

属性面板
  ├─ 特征名、类型
  ├─ 阶数 / 控制点数（P1 只读）
  └─ CV 表 / 拖动（P2）

Feature 树（若 UI 暴露）
  └─ Part → NurbsSurface1 → ...
```

**命令注册**：沿用 `CommandRegistry` / `BuiltinCommands` 模式；IoC 见 ADR 0007。

---

## 6. 依赖与风险（占位）

| 风险 | 缓解 |
|------|------|
| Trim/Sew 未实现无法 Solid | P1 限制 Sheet；P3 前专 ADR |
| CV 编辑频繁 Regenerate 卡顿 | 拖动时仅更新显示 mesh；松开再 Commit Rebuild |
| XL 控制点数组体积大 | 可选外部文件引用；或压缩存储 |
| 与布尔 Pipeline 并行抢内核人力 | 严格 Gate；建模线 Display 达标后再开 P1 |
| UI 范围蔓延（要做完整 NX FreeStyle） | 本文档 P2 明确「单曲面 CV 编辑」上限 |

---

## 7. 建议 ADR 0011 提纲（未写）

若正式立项，可新增 **ADR 0011: NURBS 建模特征与 Viewer 编辑范围**，决策点：

1. P1 是否只做 Sheet NURBS？
2. 控制点是否全部参数化进 `ParameterStore`？
3. P2 CV 拖拽是否必须 G1 约束？
4. 是否允许 OCCT 仅用于 STEP 导入 NURBS（与 ADR 0005 交叉）？

---

## 8. 验收标准（占位，按阶段）

| 阶段 | 验收 |
|------|------|
| P1 | XL roundtrip 含 `NurbsSurfaceFeature`；改参数 Regenerate 后 mesh 变；`ValidateBody` |
| P2 | 视口拖 CV → 曲面形状变；Undo 恢复 |
| P3 | Loft 两截面 → Sheet/Solid；Trim 后环闭合 |
| P4 | 曲率梳截图与参考 CAD 定性一致（无定量对标承诺） |

---

## 9. 文档索引

| 文档 | 关系 |
|------|------|
| [ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md) | 近期 Display；本文档 **不替代** |
| [NURBS 显示设计](./2026-08-24-nurbs-display-viewer-design.md) | P0 重叠 |
| [NURBS 显示计划](../plans/2026-08-24-nurbs-display-implementation.md) | 必须先推进 |
| [ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md) | NURBS 布尔更后 |
| [2026-08-10 布尔设计](./2026-08-10-analytic-sphere-brep-boolean-design.md) | 范围膨胀风险 |

---

## 10. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-08-24 | 初版占位；P0–P4 分期与 Gate 定义 |
