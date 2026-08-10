# 解析球面 + 通用 B-Rep 布尔 —— 技术实现方案

日期：2026-08-10  
状态：Phase 0 已决议（待决事项已关闭）  
分支：`cursor/modern-cpp-brep-kernel`

## 目标

1. **光滑球体**（接近常见 CAD / MicroStation 观感）：几何为**解析球面**，显示采用**公差驱动 / 自适应三角化**，并用真实曲面法向做光滑着色。
2. **通用曲面 B-Rep 布尔**（并 / 减 / 交）：自研求值器，流水线**理念对齐 OpenCASCADE**（Phase 1～N 默认**不链接、不依赖** OCCT，除非后续另行决策）。

## 近期非目标

- 链接或内嵌 OpenCASCADE 二进制
- 通用 NURBS 布尔（等平面 / 球面 / 柱面路径稳定后再做）
- 装配实例级布尔
- 完整非流形修复工具集
- 以**纯网格布尔**作为正式结果（网格仅可作调试 / 回退显示）

## 已锁定决策（产品讨论结论）

| 主题 | 选择 |
|------|------|
| 球体观感 | 解析 `SphereSurface` + 更好的显示细分 |
| 布尔目标 | 通用曲面 B-Rep 布尔（自研） |
| 网格布尔 | 非主路径；仅可选调试 |
| 外部内核 | 只参考 OCCT 理念；自行实现 |
| 布尔 UI | 选中两个对象 → 菜单 并 / 减 / 交 |
| 布尔后操作体 | **抑制（suppress）** 操作体特征；保留结果 |

---

## 现状与缺口

| 领域 | 当前状态 |
|------|----------|
| 球体 Body | `make_sphere` 用 **UV 平面三角壳**拼接 |
| 曲面 | 实体中基本只有 `PlaneSurface`；`SurfaceKind` 中 Cylinder/Nurbs 枚举未落地 |
| 曲线 | 有 `LineCurve`、`CircleCurve`（可求值）；实体边几乎全是直线 |
| 三角化 | `tessellate_body` 仅对**平面**外环做扇形剖分 |
| 布尔 | 无（无求交 / 分割 / 分类 / 缝合） |
| 特征 | Box、Sphere、Sketch、Extrude + 历史 / XL |

---

## 总体架构

```text
┌─────────────────────────────────────────────────────────────┐
│ Viewer：选中 2 个 → boolean.union|subtract|intersect          │
│ BooleanFeature + 抑制操作体 + 再生 + 撤销                      │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│ Part / Regenerator                                           │
│  BooleanFeature::rebuild → IBooleanEvaluator                 │
└───────────────────────────┬─────────────────────────────────┘
                            │
        ┌───────────────────┴───────────────────┐
        ▼                                       ▼
┌───────────────────┐                 ┌─────────────────────┐
│ 几何内核           │                 │ 布尔流水线            │
│ SphereSurface     │                 │ （对齐 OCCT 阶段）     │
│ CircleCurve       │                 │ 求交→分割→            │
│ Tessellator       │                 │ 分类→重建             │
└───────────────────┘                 └─────────────────────┘
```

---

## 部分 A —— 解析球面 + 显示细分

### A1. 几何类型

扩展 `SurfaceKind` 并实现：

```text
SurfaceKind += Sphere   // 使用独立种类，避免滥用 Nurbs

class SphereSurface : public Surface {
  Point3d center;
  double radius;
  // 局部 UV：u = 经度 [0,2π)，v = 纬度 [-π/2, π/2]
  eval(u,v), normal(u,v), param_of(point)  // 投影 / 分类用
};
```

需要时把 `CircleCurve` 作为球面缝线 / 纬线的一等公民。

### A2. 解析球的拓扑（B-Rep，不是三角壳）—— **已决议**

采用 **「双极点 + 一条经向缝边 + 单球面 Face」**（OCCT 实用球的简化版）。

**参数域**（`SphereSurface`）

- `u` ∈ `[0, 2π)`：经度（缝在 `u=0 ≡ u=2π`）
- `v` ∈ `[-π/2, +π/2]`：纬度（`v=-π/2` 南极，`v=+π/2` 北极）

**拓扑实体**

| 元素 | 说明 |
|------|------|
| `V_s`, `V_n` | 南 / 北极点（各 1 个 Vertex） |
| `E_seam` | 经向缝：南极→北极半大圆（3D 用半圆 `CircleCurve` 或等价参数曲线） |
| `F_sphere` | 1 个 Face，曲面 = `SphereSurface` |
| Outer loop | `CoEdge(+E_seam)` + `CoEdge(-E_seam)`，参数域上绕整张球面闭合 |

参数矩形展开（左右边粘成同一条 seam）：

```text
        v = +π/2  ● V_n (北极)
                  │
                  │ E_seam  (u=0 与 u=2π 为同一条边)
                  │
        v = -π/2  ● V_s (南极)

   u: 0 ───────────────────── 2π
      │←—— 同一条 seam 粘合 ——→│
```

**Phase 1 刻意不做：** 极点零长度退化边；双半球两 Face（缝稳后再议）。

**成功标准：**

- `Body` **通过封闭实体校验**
- 三角化得到光滑网格；圆心捕捉使用解析球心
- Viewer 默认**不画** seam（或极淡），避免经线疤

将 `make_sphere` / `SphereFeature::rebuild` 从平面三角壳**迁移**到该解析体。

### A3. 三角化（显示）—— **已决议默认偏差**

去掉 `tessellate_body`「只认平面」的假设。相对半径 `R` 归一，并夹绝对下限：

| 参数 | 默认值 | 含义 |
|------|--------|------|
| `linear_deflection` | `max(0.02 * R, 1e-4)` | 弦高 ≈ 半径 2% |
| `angular_deflection` | `15°`（≈ 0.26 rad） | 相邻法向夹角上限 |
| `min_u_segments` / `min_v_segments` | `24` / `12` | 经 / 纬最少段数 |
| `max_u_segments` / `max_v_segments` | `128` / `64` | 防止过大网格 |

可选档位（Phase 1 可先写死默认，设置项后加）：草稿 `0.05*R` + `20°`；精细 `0.01*R` + `10°`。

```text
TessellationOptions { linear_deflection; angular_deflection; min/max_segments_u/v; }

tessellate_face(Face) →
  if Plane → 现有扇形剖分
  if Sphere → UV 网格 / 递归细分直到满足偏差
             法向取自 SphereSurface::normal（光滑着色）
```

Viewer 继续上传 `TriangleMesh`，使用**真实曲面法向**。`SphereSpec.slices/stacks` 在解析球落地后仅作兼容/调试；正式显示走 `TessellationOptions`。

### A4. 边线显示

球面的 `extract_edges`：可画 seam，或近似轮廓线。**已决议：** Phase A 默认隐藏 / 极淡 seam。

### A5. 捕捉 / 属性

- 圆心捕捉：`SphereFeature` / `SphereSurface` 球心
- 半径参数不变
- 属性面板功能不变

### A6. 部分 A 验收

- 默认偏差下，正交 / 透视中球体观感光滑
- 放大后通过更紧公差仍可接受
- XL 往返 + 改半径能再生解析球
- 流形校验通过

---

## 部分 B —— 通用 B-Rep 布尔（理念对齐 OCCT，自研）

### B0. 与 OCCT 概念映射

| OCCT 概念 | 本项目模块 |
|-----------|------------|
| `BRepAlgoAPI_Fuse/Cut/Common` | `BooleanFeature` + `BooleanOp` 枚举 |
| Intersection（求交） | `IntTools` / `SurfaceIntersector` |
| Split / imprint（分割 / 印记） | `FaceSplitter`、`EdgeSplitter` |
| Solid classifier（实体分类） | `SolidClassifier`（IN / OUT / ON） |
| Builder（重建） | `BooleanBuilder` 选面缝合成 Shell / Body |
| Tolerances（容差） | `BooleanContext { fuzzy, tol_3d, tol_2d }` |

**禁止复制 OCCT 源码。** 用清晰接口重实现各阶段；文档中引用公开 OCCT 资料 / 教材中的布尔流水线即可。

### B1. 特征与 UI（与求值器同期交付）

```text
BooleanFeature {
  op: Union | Subtract | Intersect
  target_feature_id
  tool_feature_id
}

UI：建模菜单 + 工具栏 —— 要求恰好选中 2 个 Body/特征
减：主选择 = 目标，次选择 = 工具体
成功后：抑制两个操作体；显示结果；撤销可恢复
XL：持久化 BooleanFeature + 抑制标志
```

### B2. 求值器接口

```text
struct BooleanResult {
  Body* body;               // 由 Model/Part 拥有
  BooleanEvalMode mode;     // AnalyticPair | General
  std::string diagnostics;
};

class IBooleanEvaluator {
  virtual BooleanResult evaluate(BooleanOp, const Body& a, const Body& b,
                                 const BooleanContext&) = 0;
};
```

调度器可对 **AABB 盒子 ∪/−/∩ AABB 盒子** 做正确性 / 性能快路径（仍输出 B-Rep）。

### B3. 通用流水线阶段

```text
1. 预处理 Preprocess
   - 将操作体变换到同一坐标系（当前多为单位变换）
   - 收集面/边；建立包围盒

2. 几何求交 Intersection
   - 面–面：Plane–Plane、Plane–Sphere、Sphere–Sphere（按优先级）
   - 产出三维交线 + 双方 pcurve
   - 必要时边–面求交补全图

3. 拓扑分割 / 印记 Split / Imprint
   - 边上插入顶点
   - 沿交线分割边 / 面
   - 更新环（Outer / Inner）

4. 分类 Classification
   - 每个面（或面片）相对另一实体：IN / OUT / ON
   - 优先用解析内外（有符号距离）/ 射线 / 绕数

5. 按运算选面 Selection
   - 并：按标准 CSG 保留规则保留 OUT∪ON 等
   - 减（A−B）：保留 A 在 B 外的面 + B 在 A 内的面（方向取反）
   - 交：保留位于对方内部的面

6. 重建 Build
   - 配对 partner、定向 Shell、创建 Body
   - validate_body 流形检查

7. 软失败 Fail soft
   - 空结果、非流形、不支持的曲面组合 → CommandResult::failed 并给出原因
```

### B4. 几何支持矩阵（部分 B 内再分期）

| 曲面组合 | 阶段 |
|----------|------|
| Plane–Plane（盒–盒、拉伸–盒） | B.1 |
| Plane–Sphere | B.2 |
| Sphere–Sphere | B.2 |
| Cylinder–* | B.3（先落地 `CylinderSurface`） |
| NURBS–* | 更后 |

**部分 A（解析球）是 B.2 的硬前置。**

### B5. 容差

- 全局 `BooleanContext::fuzzy` 处理近重合
- 顶点合并距离；无解析闭式时对交线做曲线采样
- 平面 / 球面优先用解析求交公式，再考虑数值推进

### B6. 测试策略

| 层级 | 用例 |
|------|------|
| 单元 | 平面–平面交线；平面–球面圆；球–球圆 / 点 / 空 |
| Body | 盒∪盒、盒−盒、盒∩盒 → 校验 + 体积 / AABB 合理性 |
| Body | 球−盒、球∪球、盒∩球 |
| 特征 | 改操作体半径/尺寸后 BooleanFeature 再生；撤销/重做；XL |
| Viewer | 手工：选中两对象 → 运算；操作体被抑制；球显示光滑 |

### B7. 部分 B 验收（通用布尔）

- 平面实体（盒子 / 拉伸）的并/减/交稳定
- 至少一个弯曲用例（球 vs 盒 或 球 vs 球）产出通过校验的实体
- 失败必须显式（禁止静默损坏拓扑）
- 构建不依赖 OCCT

---

## 交付分期（执行顺序）

### Phase 0 —— 方案冻结（**已完成**）

- [x] 解析球缝 / 极点拓扑方案（§A2）
- [x] Viewer 默认偏差（§A3）
- [x] Inner loop 与 Phase 2.0 / 2.1 分期（§交付分期 + 文末决议表）
- 布尔各阶段接口维持本文 §B 定义，实现时如无冲突不再重开 Phase 0

### Phase 1 —— 解析球 + 三角化（部分 A）

1. `SphereSurface` + `Model` 工厂 / 类型
2. 重写 `make_sphere` 为解析 B-Rep
3. 扩展 `tessellate_body`（偏差选项 + 光滑法向）
4. 迁移 viewer / 创建球工具（`SphereSpec` API 尽量不破坏）
5. 测试：几何求值、网格密度、校验、XL、视觉冒烟

**退出标准：** Viewer 中球体观感接近光滑 CAD 球。

### Phase 2 —— 布尔脚手架 + 平面实体布尔（部分 B.1）

拆成两档（**Inner loop 决议**见下）：

**Phase 2.0（可不依赖 Inner loop）**

1. `BooleanFeature`、历史、抑制操作体、XL
2. Viewer 命令 / 菜单（双对象选择）
3. 平面–平面求交 + **盒子**布尔重建（结果面可全为 Outer）
4. 可选 AABB 盒子快路径（同一 API）
5. Kernel gtest + Viewer 冒烟

**退出标准：** 盒子真 B-Rep 并/减/交可用。

**穿插专项（Phase 2.0 后、2.1 / 3 前必须完成）**

- 支持 `LoopType::Inner` + 拉伸带孔（extrude holes）
- 原因：复杂 Cut / 面印记后交线常形成内环；无 Inner 无法表达「面上有洞」

**Phase 2.1（依赖 Inner loop）**

- 平面拉伸体参与布尔、面上开孔类 Cut 结果

### Phase 3 —— 弯曲求交 + 球面布尔（部分 B.2）

1. 平面–球 / 球–球交线
2. 球面印记；基于解析内外的分类器
3. 球−盒 / 球∪球 演示
4. 加固容差与失败诊断

**退出标准：** 弯曲布尔可用于学习演示；仍非完整商用 CAD 内核。

### Phase 4 —— 扩展曲面（可选）

- `CylinderSurface` 与更多求交对
- 更好的 pcurve、命名（`TopologyRef`）、性能（BVH）

### Phase 5 —— 决策门

- 继续自研走向 NURBS，**或**
- 在 `IBooleanEvaluator` 后增加可选 OCCT 后端（UI / 特征不变）

---

## 规划模块 / 文件映射

```text
kernel/include/brep/geometry.hpp          # SphereSurface
kernel/include/brep/types.hpp             # SurfaceKind::Sphere
kernel/src/builder.cpp                    # 解析 make_sphere
kernel/src/mesh.cpp                       # 多曲面三角化
kernel/include/brep/bool/...              # context、op、result
kernel/src/bool/intersect_*.cpp
kernel/src/bool/split_*.cpp
kernel/src/bool/classify.cpp
kernel/src/bool/build.cpp
kernel/include/brep/feat/boolean_feature.hpp
kernel/src/feat/boolean_feature.cpp
apps/viewer/commands/...                  # 布尔命令
apps/viewer/ui/main_window_menus.cpp      # 菜单 / 工具栏
docs/superpowers/specs/...                # 本文档 + 短 ADR
```

---

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 球极点 / 缝拓扑脆弱 | 先固定规范布局；加重 validate 测试 |
| 通用布尔周期长 | 严格阶段退出；先平面后弯曲 |
| 数值不稳定 | 优先解析公式；fuzzy 容差；拒绝不支持组合 |
| 范围膨胀（过早 NURBS） | 明确放到 Phase 4+ |
| 旧 XL 中的三角壳球 | 版本升级或加载时对 Sphere 特征重建 |

## 工作量粗估（示意）

| 阶段 | 粗估 |
|------|------|
| Phase 1 解析球 + 细分 | 1～3 周 |
| Phase 2 布尔特征 + 平面布尔 | 4～8 周 |
| Phase 3 弯曲布尔 MVP | 6～12+ 周 |
| Phase 4+ | 持续 |

（取决于鲁棒性要求与测试深度。）

---

## 项目级成功标准

1. 球体以解析法向 + 偏差驱动网格光滑显示。
2. 用户可通过「选中 + 菜单」对两实体做并/减/交；操作体被抑制；撤销可用。
3. 平面实体布尔得到通过校验的 B-Rep；至少一个弯曲布尔用例端到端跑通。
4. 流水线阶段可分离，并文档化与 OCCT 概念的映射（构建不强制依赖 OCCT）。

## Phase 0 已决议（原待决事项，已关闭）

| 事项 | 决议 |
|------|------|
| 球面缝 / 极点拓扑 | **双极点 + 单经向 seam + 单球面 Face**；参数域 `u∈[0,2π)`、`v∈[-π/2,π/2]`；详见 §A2 |
| Viewer 默认偏差 | `linear = max(0.02R, 1e-4)`，`angular = 15°`，经/纬最少 24/12、最多 128/64；详见 §A3 |
| Phase 2 前 Inner loop | **2.0 盒布尔不强制**；**2.1 / 弯曲 Cut 前必须**完成 Inner loop + extrude holes 专项 |

无未关闭的 Phase 0 待决项。

---

## 任务拆解（按需求落地）

> 用法：实现时按编号顺序推进；`- [ ]` 表示未完成。每个任务应有可验证出口（测试或手工验收）。  
> 依赖：Phase 1 → Phase 2.0 →（Inner 专项）→ Phase 2.1 → Phase 3；Phase 4/5 可选。

### 总览

| 阶段 | 任务 ID | 主题 | 出口 |
|------|---------|------|------|
| 0 | T0 | 方案冻结 | 已完成 |
| 1 | T1.1～T1.6 | 解析球 + 显示细分 | 光滑球可用 |
| 2.0 | T2.0.1～T2.0.7 | 布尔特征 + 盒布尔 | 盒并/减/交真 B-Rep |
| 2.x | T2.x.1～T2.x.3 | Inner loop + 拉伸孔 | 面可带内环 |
| 2.1 | T2.1.1～T2.1.3 | 平面拉伸参与布尔 | 拉伸 Cut/并可用 |
| 3 | T3.1～T3.6 | 弯曲求交 + 球面布尔 | 至少一弯曲用例端到端 |
| 4 | T4.* | 柱面等扩展 | 可选 |
| 5 | T5.* | 自研 vs OCCT 决策 | 可选 |

---

### Phase 0 —— 方案冻结

- [x] **T0.1** 锁定球面缝 / 极点拓扑（§A2）
- [x] **T0.2** 锁定 Viewer 默认偏差（§A3）
- [x] **T0.3** 锁定 Inner loop 与 Phase 2.0/2.1 分期
- [x] **T0.4** 冻结布尔流水线接口（§B2～B3）

---

### Phase 1 —— 解析球面 + 显示细分（部分 A）

#### T1.1 `SphereSurface` 几何类型

- [x] 扩展 `SurfaceKind`，增加 `Sphere`
- [x] 实现 `SphereSurface`：`center`、`radius`、`eval(u,v)`、`normal(u,v)`、`param_of`
- [x] UV 约定：`u∈[0,2π)`，`v∈[-π/2,π/2]`
- [x] `Model` 增加工厂（如 `make_sphere_surface`）
- [x] **测试：** 赤道/极点求值与法向；已知点 `param_of` 往返（`brep_test_sphere_surface`）

**主要文件：** `kernel/include/brep/types.hpp`、`geometry.hpp`、`model.hpp`、对应 `.cpp`、`tests/kernel/test_sphere_surface.cpp`

#### T1.2 解析球 B-Rep 构建（替换三角壳）

- [x] 按 §A2 构建：`V_s`/`V_n` + `E_seam` + 单 `F_sphere` + Outer（±seam）
- [x] 重写 `make_sphere`；`SphereFeature::rebuild` / `Part::rebuild_sphere_body` 走新路径
- [x] `SphereSpec.slices/stacks` 降级为兼容/调试字段（可不驱动拓扑）
- [x] **测试：** `validate_body` 封闭实体；旧 XL Sphere 加载可再生

**主要文件：** `kernel/src/builder.cpp`、`feat/sphere_feature.cpp`、`part.cpp`

#### T1.3 多曲面三角化 + 默认偏差

- [ ] 引入 `TessellationOptions`（§A3 默认表）
- [ ] `tessellate_body`：Plane 保持扇形；Sphere 按偏差 UV/细分，法向取自曲面
- [ ] 遵守 min/max 经纬段数
- [ ] **测试：** 默认偏差下网格规模合理；法向与解析法向方向一致（点抽样）

**主要文件：** `kernel/include/brep/mesh.hpp`、`kernel/src/mesh.cpp`、`api/mesh.hpp`

#### T1.4 边线显示（隐藏 seam）

- [ ] `extract_edges` / Viewer：默认不画或极淡 seam
- [ ] 可选调试开关显示 seam（可后置）
- [ ] **验收：** 视口中无明显经线疤

**主要文件：** `mesh.cpp`、viewer 边线上传/渲染相关

#### T1.5 Viewer / 工具 / 捕捉迁移

- [ ] 创建球工具、同步、属性面板仍用 `SphereSpec`（半径/球心）
- [ ] 圆心捕捉继续走解析球心（`SphereFeature` / `SphereSurface`）
- [ ] **验收：** 交互创球、改半径、AccuSnap 圆心正常

**主要文件：** `create_sphere_tool.cpp`、`scene_adapter`、`property_panel`、snap 相关

#### T1.6 Phase 1 验收门禁

- [ ] Kernel：几何 / 构建 / 三角化 / validate / XL 自动化测试通过
- [ ] Viewer：正交+透视下球观感光滑；放大可接受
- [ ] 文档：Phase 1 退出标准勾选完成

---

### Phase 2.0 —— 布尔特征 + 盒子真 B-Rep 布尔

#### T2.0.1 布尔类型与求值器骨架

- [ ] 定义 `BooleanOp`、`BooleanContext`、`BooleanResult`、`IBooleanEvaluator`
- [ ] 目录骨架：`kernel/include/brep/bool/`、`kernel/src/bool/`
- [ ] **测试：** 空壳可链接；不支持组合返回明确失败

#### T2.0.2 `BooleanFeature` + 抑制操作体

- [ ] `BooleanFeature`：`op`、`target_feature_id`、`tool_feature_id`
- [ ] `rebuild` 调用求值器；成功后 suppress 两操作体
- [ ] 接入 `FeatureTree` / `Regenerator` / `FeatureHistory`（撤销恢复抑制状态）
- [ ] **测试：** 添加布尔特征后操作体不可见、结果可见；undo/redo

**主要文件：** `boolean_feature.hpp/.cpp`、`feature_history.*`、`part.*`、`CMake BrepFeat`

#### T2.0.3 XL 持久化

- [ ] 序列化/反序列化 `BooleanFeature` + 抑制标志
- [ ] **测试：** xl_roundtrip 含布尔节点

**主要文件：** `xl_document.cpp`

#### T2.0.4 平面–平面求交（最小 IntTools）

- [ ] Plane–Plane → 交线（或平行/重合诊断）
- [ ] **测试：** 正交平面交线；平行无交；重合 fuzzy 行为

**主要文件：** `kernel/src/bool/intersect_plane_plane.cpp` 等

#### T2.0.5 盒子布尔重建（并/减/交）

- [ ] 实现盒–盒 Fuse/Cut/Common → 合法 B-Rep Shell（面均可 Outer）
- [ ] 可含 AABB 快路径，但出口仍为 B-Rep
- [ ] **测试：** 三种运算 + `validate_body`；空结果/无交失败有诊断

#### T2.0.6 Viewer：双选 + 菜单/工具栏

- [ ] 命令：`boolean.union` / `boolean.subtract` / `boolean.intersect`
- [ ] 恰好 2 选；减：主选=目标，次选=工具
- [ ] 菜单 + 工具栏 + `tr` / `xcad_zh_CN.ts`
- [ ] SceneAdapter / 属性：识别 Boolean 类型（可只读显示 op）
- [ ] **验收：** UI 完成盒并/减/交；操作体被抑制

**主要文件：** `builtin_commands.cpp`、`main_window_menus.cpp`、`scene_adapter.*`、i18n

#### T2.0.7 Phase 2.0 验收门禁

- [ ] Kernel 盒布尔套件通过
- [ ] Viewer 手工冒烟通过
- [ ] 不依赖 Inner loop

---

### 穿插专项 —— Inner loop + 拉伸孔（2.0 后、2.1/3 前）

#### T2.x.1 拓扑与校验支持 Inner

- [ ] Face 可挂 Outer + 一个或多个 Inner
- [ ] `validate` / `link_loop` 规则覆盖内环
- [ ] **测试：** 构造带孔平面面并通过校验

#### T2.x.2 拉伸带孔（extrude holes）

- [ ] `Profile2d::holes` 真正参与 `extrude` 生成内环
- [ ] **测试：** 带孔轮廓拉伸为有洞的实体（或有洞的面）

#### T2.x.3 三角化支持内环

- [ ] `tessellate_body` 正确剖分 Outer+Inner（耳切/约束三角等）
- [ ] **测试：** 带孔面网格无盖洞、无自交明显错误

---

### Phase 2.1 —— 平面拉伸参与布尔

#### T2.1.1 平面实体通用分割/选面（在 2.0 求交之上）

- [ ] 边/面沿交线分割；结果可含 Inner
- [ ] **测试：** 简单拉伸−盒 或 盒−拉伸 产生合法体

#### T2.1.2 分类器（平面实体）

- [ ] 面片相对另一实体 IN/OUT/ON（解析或射线）
- [ ] **测试：** 已知构型分类正确

#### T2.1.3 Phase 2.1 验收

- [ ] 至少一种「拉伸参与」的并/减/交端到端通过 validate

---

### Phase 3 —— 弯曲求交 + 球面布尔（部分 B.2）

> **前置：** Phase 1 完成；建议 Inner 专项已完成。

#### T3.1 Plane–Sphere 求交

- [ ] 交为圆 / 点 / 空；解析公式优先
- [ ] **测试：** 单位球与平面的典型构型

#### T3.2 Sphere–Sphere 求交

- [ ] 交为圆 / 点 / 空 / 重合诊断
- [ ] **测试：** 分离、相切、相交、包含

#### T3.3 球面印记 + 环更新

- [ ] 交线印到球面；更新 loops（必要时 Inner）
- [ ] **测试：** 印记后球面拓扑可校验

#### T3.4 解析分类（球/盒）

- [ ] 利用球内外（到球心距离）与平面半空间
- [ ] **测试：** 盒面相对球、球面相对盒的分类抽样

#### T3.5 端到端弯曲布尔

- [ ] 至少打通：`Sphere−Box` **或** `Sphere∪Sphere` **或** `Box∩Sphere` 之一
- [ ] Viewer 可选演示路径
- [ ] **验收：** 结果 `validate_body` 通过；失败有明确诊断

#### T3.6 Phase 3 验收门禁

- [ ] 弯曲单元测试 + 至少 1 个 Body 级弯曲用例通过
- [ ] 文档勾选 Phase 3 退出标准

---

### Phase 4 —— 扩展（可选，不阻塞主线）

- [ ] **T4.1** `CylinderSurface` + 工厂
- [ ] **T4.2** Cylinder–Plane / Cylinder–Sphere 求交
- [ ] **T4.3** 改进 pcurve / `TopologyRef` 命名
- [ ] **T4.4** 求交加速结构（BVH）

---

### Phase 5 —— 决策门（可选）

- [ ] **T5.1** 评估自研 NURBS 路径 vs 可选 OCCT 后端
- [ ] **T5.2** 若选 OCCT：在 `IBooleanEvaluator` 后增加适配器，UI/特征不变
- [ ] **T5.3** 形成书面决策（短 ADR）

---

### 跨切任务（全程约束）

- [ ] **TX.1** 禁止引入 OCCT 链接（除非执行 T5.2）
- [ ] **TX.2** 布尔失败必须软失败 + 可读诊断，禁止静默坏拓扑
- [ ] **TX.3** 用户可见字符串 `tr()` + `xcad_zh_CN.ts`
- [ ] **TX.4** 每个 Phase 退出前跑约定 ctest / 冒烟清单
- [ ] **TX.5** 提交信息按阶段语义化（勿混杂无关文件）

---

### 建议实施顺序（第一条可执行路径）

```text
T1.1 → T1.2 → T1.3 → T1.4 → T1.5 → T1.6
  → T2.0.1 → T2.0.2 → T2.0.3 → T2.0.4 → T2.0.5 → T2.0.6 → T2.0.7
  → T2.x.1 → T2.x.2 → T2.x.3
  → T2.1.* → T3.* → (T4/T5 按需)
```
