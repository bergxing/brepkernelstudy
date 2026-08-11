# 裁剪面细分：自研参数域 CDT

**日期**：2026-08-11  
**状态**：**已实现**（2026-08-11；自研参数域 CDT 替换平面 ear-clip / 整球 UV 网格）  
**触发**：`untitled.xl` 中球∪盒（`33e92047-…` ∪ `cba4ae3a-…`）布尔拓扑通过，但显示异常——整球网格 + 带贴角内环的平面填充失败。  
**关联**：
- 布尔设计 [2026-08-10-analytic-sphere-brep-boolean-design.md](./2026-08-10-analytic-sphere-brep-boolean-design.md)（Phase 1 细分；T2.x Inner；T3.7 Sphere∪Box）
- [ADR 0005](../../architecture/adr/0005-boolean-backend-self-hosted.md)（继续自研，不引入第三方剖分库）

---

## 1. 问题

| 现象 | 根因 |
|------|------|
| `tessellate_body` 约 364 verts / 541 tris | `tessellate_sphere_face` **忽略 Outer 环**，生成整球 UV 网格 |
| 盒面线框碎、材质只剩零星三角 | 平面 `collect_loop_ring` 只取环顶点；贴角孔与 Outer **共享角点**，现有 `bridge_hole` + ear-clip 假定孔严格在内部 |

B-Rep 本身约 7 面（6 平面 + 1 球面），不是「面数暴增」；坏在**显示细分**。

---

## 2. 目标与非目标

### 目标

- 平面与球面在**参数域**用同一套自研 **CDT**（约束德劳内三角剖分）：环边为约束边，三角化后映射回 3D。
- 支持：多 Outer、多 Inner、贴顶点/贴边孔、直线与圆弧（及一般曲线）环采样。
- 球面 Outer 裁剪正确（不再画整球）；`untitled.xl` 角点球∪盒显示为「盒 + 外凸球面片」。
- `validate` 允许多 Outer；提供 `Face::outer_loops()`。

### 非目标（本轮）

- 不引入第三方剖分库（earcut / poly2tri / Triangle 等）。
- 不改布尔算法本身；不做 NURBS。
- 圆柱：预留同一 CDT 管线接口，**实现可第二批**。
- 不追求最优网格质量（Steiner 点 / 质量细化可后续加）。

### 已选方案

评估过三条路径后锁定 **方案 2：自研参数域 CDT**（非「加固 ear-clip + UV 网格分类」的渐进方案）。理由：多环、贴边孔、后续细化与柱面扩展都更适合约束三角，一次性把剖分内核做对。

---

## 3. 拓扑变更

| 项 | 现状 | 目标 |
|----|------|------|
| Outer 数量 | `validate` 要求恰好 1 | **至少一个** Outer |
| `Face::outer_loop()` | 返回唯一 Outer | 保留：返回**第一个** Outer（兼容旧调用） |
| `Face::outer_loops()` | 无 | 新增：返回全部 Outer |
| Inner | 已支持多个 | 不变；可与 Outer 贴顶点/贴边 |

**多 Outer 归属**：每个 Outer 与其归属 Inner 组成一个「面片区域」，分别 CDT。归属规则：Inner 在 UV 上的质心落在哪个 Outer 多边形内（点在多边形内；边界算归属该 Outer）。若无法归属 → `validate`/细分诊断警告并跳过该 Inner。

---

## 4. CDT 是什么、数据结构是什么

### 4.1 概念

**CDT** = Constrained Delaunay Triangulation（约束德劳内三角剖分）。

给定平面（此处为曲面**参数域 UV**）上的一些**点**和若干必须保留的**约束边**，输出一组三角形，使得：

- 每条约束边都是某条三角形边（或细分后的共线边链）；
- 在无约束时尽量满足 Delaunay（外接圆内尽量不进别的点）；有约束时在不破坏约束边的前提下尽量接近。

对裁剪面：Outer / Inner 经采样得到的折线边就是约束边；CDT 填满「在 Outer 内且不在任何所属 Inner 内」的区域。

与当前 **ear-clip** 的差别：ear-clip 是「单个简单多边形直接剪耳朵」；CDT 是「先有点 + 强制边，再长出三角网」，对多环、贴边孔、后续加 Steiner 点细化更合适。

### 4.2 建议数据结构

| 结构 | 存什么 |
|------|--------|
| **Vertex** | UV `(u,v)`；可选回指 3D 位置缓存、拓扑 `Vertex*` / 采样来源 |
| **HalfEdge** | 起点、下一条、对偶、所属三角形；便于绕顶点 / 绕面遍历 |
| **Triangle** | 三条 half-edge（或三个顶点下标） |
| **ConstraintEdge** | 环上必须保留的边（相邻采样点对）；插入后标记 constrained |
| **CdtMesh** | 顶点表 + 三角形表 + 约束边集合；可选定位结构（行走 / 桶）加速「点在哪一三角」 |

### 4.3 算法步骤（逻辑）

1. 插入所有采样点（及缝复制点，见 §5.3）。
2. 插入 / 恢复每条约束边（必要时在边上加点细分以满足空圆或数值稳定）。
3. 删除 Outer 外、以及任一所属 Inner 内的三角形。
4. 将留下的三角形顶点 `surface.eval(u,v)` 映回 3D；法向 `face.normal_at`；贴图 UV 归一化到该面局部包围盒。

实现算法（增量 Lawson / Bowyer–Watson 变体 / 分治）在实现计划中选定；本规格只锁定**接口与不变量**：约束边保留、输出三角形覆盖面的有界区域且不覆盖孔。

---

## 5. 细分流水线

### 5.1 统一入口

```
tessellate_face(face, opts)
  → sample loops (3D → UV)
  → group regions (outer + inners)
  → build constraints
  → CDT per region
  → append TriangleMesh (3D + normals + uv)
```

公开 API：`tessellate_face` / `tessellate_body` / `TessellationOptions` **签名不变**。

### 5.2 环采样

- 按 `TessellationOptions`（`linear_deflection` / `angular_deflection`）对每条 `Edge` 采样。
- 直线：端点即可（长边可按弦高加密）。
- `CircleCurve` / 弧：按角步长；保证弦高 ≤ deflection。
- 只取拓扑顶点不够（Sphere∪Box 的四分之一圆弧必须采样）。
- 平面：`PlaneSurface::param_of`；球面：`SphereSurface::param_of`。

### 5.3 球面缝与极点

- 球 UV：`u ∈ [0, 2π)`，`v ∈ [-π/2, π/2]`，`u = 0` 为缝。
- 相邻采样点若 `|Δu| > π`，视为跨缝：将该段拆到缝两侧，复制缝上顶点（`u = 0` 与 `u = 2π`）。
- 裁剪环跨缝时：在参数域切成不跨缝的多边形后再分别 / 联合进入 CDT（同一区域可多块不跨缝约束多边形，再用缝边约束粘合或分片剖分后拼接）。
- 极点：用小 ε 避开 `v = ±π/2` 奇异，或把极点表示为显式顶点并约束经线。

### 5.4 贴边 / 贴角孔

- 共享顶点合法；CDT 中同一 UV 点合并（容差）。
- 共线约束边合并，禁止对贴边孔做「零宽 bridge」式构造。
- 验收：角点孔区域无三角覆盖；其余盒面有连续填充。

### 5.5 平面

- UV = 平面参数，无缝，直接 CDT。
- 替换现有 `bridge_hole` + `ear_clip_triangulate` 路径（可暂时保留旧代码作对照测试，合并前删除或 `#if` 关掉）。

---

## 6. API 与文件布局

### 6.1 API

- `Face::outer_loops() const → std::vector<Loop*>`
- `Face::outer_loop()`：第一个 Outer（可空）
- `Face::inner_loops()`：保持现有
- `validate_body`：Outer 个数 `>= 1`；删除「exactly one」错误

### 6.2 文件（建议）

| 路径 | 职责 |
|------|------|
| `kernel/include/brep/mesh/cdt.hpp` | CDT 数据结构与 `triangulate_constrained` |
| `kernel/src/mesh/cdt.cpp` | CDT 实现 |
| `kernel/src/mesh/loop_sample.cpp` | 边采样、球面缝展开、区域分组 |
| `kernel/include/brep/mesh/loop_sample.hpp` | 采样 API（内部 / 测试可见） |
| `kernel/src/mesh.cpp` | 编排；平面 / 球面走同一管线 |
| `kernel/include/brep/topology.hpp` + `topology.cpp` | `outer_loops` |
| `kernel/src/validate.cpp` | 多 Outer 规则 |
| `tests/kernel/test_cdt.cpp` | CDT 单元 |
| `tests/kernel/test_tessellate_trimmed_sphere.cpp` | 球面裁剪 |
| `tests/kernel/test_tessellate_inner.cpp` | 扩展贴角 / 多孔 |
| `tests/kernel/test_inner_loop.cpp` | 多 Outer validate |

圆柱第二批：`CylinderSurface::param_of` 已存在，采样器加 `SurfaceKind::Cylinder` 即可接入同一 CDT。

---

## 7. 测试与验收

### 7.1 门禁

1. **多 Outer**：构造双 Outer 面 → `validate` 通过；旧单 Outer 回归仍绿。
2. **矩形 + 内孔**：孔心无三角形；顶面面积近似（保留现有 `TessellateInner.HoleCenterNotCovered`）。
3. **贴角孔**：模拟 Sphere∪Box 平面环（含圆弧采样）→ 孔区无覆盖、面有填充。
4. **球面裁剪**：Outer 为三圆弧的 ⅞ 片 → 三角形重心在环内；顶点数明显小于整球默认细分。
5. **`untitled.xl` 构型回归**（kernel 测试，不依赖 viewer）：
   - 盒 `min≈(-2.38421,0,4.59474)` `max≈(-1.57147,0.826297,5.26894)`
   - 球 `center≈(-1.57147,0.826297,4.59474)` `r≈0.413149`
   - Union 后细分：平面三角合理；球面非整球；可选检查「盒角点外侧八分之一球方向有三角 / 内侧八分被裁掉」。

### 7.2 非回归

- `brep_test_tessellate_sphere`、`brep_test_tessellate_inner`
- 盒 / 球布尔、`viewer_adapter_tests` 冒烟

### 7.3 成功标准（产品）

重启 viewer，打开 `C:/Users/xingbl/Desktop/untitled.xl`，对上述两 GUID Fuse：结果看起来是完整盒体 + 角点外凸球面，而非「完整球 + 碎盒面」。

---

## 8. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 自研 CDT 数值不稳 / 约束边插入失败 | 容差统一；失败时诊断 + 单测覆盖贴边与跨缝；必要时边上加点 |
| 球面缝复制点导致裂缝 | 缝两侧顶点 3D 重合；渲染可接受微缝，拓扑显示边仍用 `extract_edges` |
| 工期长于加固 ear-clip | 分里程碑：CDT 平面多孔 → 贴边 → 球面缝 → untitled 回归 |
| 多 Outer 归属歧义 | 质心规则写死；歧义告警不静默丢面 |

---

## 9. 决议摘要

| 项 | 选择 |
|----|------|
| 范围 | 通用裁剪细分（多孔、贴边、球面裁剪）+ 多 Outer 拓扑 |
| 剖分 | 纯自研参数域 CDT |
| 依赖 | 无第三方剖分库 |
| 圆柱 | 接口预留，实现第二批 |
| 验收锚点 | `untitled.xl` 两 GUID 并集显示 |

---

## 10. 后续

1. 本规格审阅通过后，用 writing-plans 拆实现计划（CDT 核心 → 平面 → 球面 → 多 Outer → 回归）。
2. 实现完成后可在布尔设计文档 Phase 1 / T2.x 细分条目处交叉引用本规格。
