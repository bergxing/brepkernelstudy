# 布尔通用 Pipeline（仅通用路径）设计规格

**日期**：2026-08-24  
**状态**：已接受  
**决策**：[ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md)  
**关联**：[ADR 0006](../../architecture/adr/0006-boolean-pipeline-architecture.md)、[2026-08-14 Pipeline 规格](./2026-08-14-boolean-pipeline-design.md)、[2026-08-10 布尔设计](./2026-08-10-analytic-sphere-brep-boolean-design.md)

---

## 1. 目标

| 目标 | 说明 |
|------|------|
| **单一求值路径** | `IBooleanEvaluator` → `BooleanPipeline` 六阶段；无 `IAnalyticFastPath` |
| **对称 CSG** | `A−B` 仅由操作数顺序决定；与体类型无关 |
| **任意操作数** | 操作数可为原始基元或上次布尔结果（一般 B-Rep） |
| **按曲面扩展** | 新形体 = 新 `SurfaceKind` + `Intersect*` + Imprint；不新增体对体 Boolean 类 |
| **可观测** | 每 Stage 失败返回 `PipelineStage` 名 + 诊断 |
| **未来 Split** | Imprint 阶段可复用于「体被曲面裁剪」 |

---

## 2. 顶层结构

```text
BooleanFeature / Part / Viewer
        │
        ▼
  IBooleanEvaluator
        │
        ▼
  PipelineBooleanEvaluator     （原 Composite 去掉 FastPath）
        │
        ▼
  BooleanPipeline
        │
        ├─ PreprocessStage
        ├─ IntersectStage      ──► IntersectorRegistry + FaceBvh
        ├─ ImprintStage        ──► ImprintEngine
        ├─ ClassifyStage       ──► SolidClassifier
        ├─ SelectStage         ──► FaceSelector (CSG 规则)
        └─ BuildStage          ──► BooleanBuilder + TopologyCopy
```

---

## 3. 模块归属：删除 / 保留 / 新建

### 3.1 删除（M4 里程碑）

| 模块 | 说明 |
|------|------|
| `FastPath.h/.cpp` | `IAnalyticFastPath` 注册表 |
| `BoxBoolean.h/.cpp` | 盒×盒 grid CSG 特解 |
| `SphereBoxBoolean.h/.cpp` | 球×盒特解（⅛/⅞/角点并集等） |
| `SphereSphereBoolean.h/.cpp` | 球×球特解 |
| `PlanarBoolean.h/.cpp` | 棱柱×盒特解 |

`Boolean.h` umbrella 移除上述 include。`BooleanEvalMode::AnalyticPair` 废弃。

### 3.2 保留并升级

| 模块 | 变更 |
|------|------|
| `IntersectPlanePlane/Sphere/Cylinder/...` | 从 **Probe** 升级为返回 **3D 交线段/圆弧** |
| `IntersectorRegistry` | `Intersect(Face, Face)` 产出几何，不仅统计 nonempty |
| `Broadphase` + `FaceBvh` | 宽相候选面–面对 |
| `FaceSelector` | CSG 选面（已实现，输入改为 Imprint 后面片） |
| `SolidClassifier` | **唯一** Classify 路径：`ClassifyPointInBody`（见 §5.4） |
| `ClassifyPointInBox/Sphere/Prism` | **不接入 Pipeline**；仅保留给解析几何/Recognize **单元测试** 或后续删除 |
| `BoxRecognize` / `SphereRecognize` | 特解删除后 **仅测试/诊断**；Classify / Imprint / Evaluator **不得调用** |
| `BooleanPipeline` | 六阶段完整实现 |

### 3.3 新建

| 模块 | 职责 |
|------|------|
| `IntersectionGraph.h/.cpp` | 交点、交边、面–面对应；Intersect 阶段产出 |
| `ImprintEngine.h/.cpp` | 沿交线切分 Edge / Face |
| `SolidClassifier.h/.cpp` | `ClassifyPointInBody`：射线法 + fuzzy ON |
| `BooleanBuilder.h/.cpp` | 复制选面、翻转 B、缝 Shell、建 Body |
| `TopologyCopy.h/.cpp` | Face 子图 deep copy 到目标 `Model` |

---

## 4. PipelineState 扩展

| 字段 | 产出 Stage | 用途 |
|------|------------|------|
| `Op`, `BodyA`, `BodyB`, `Ctx`, `TargetModel` | 输入 | 运算上下文 |
| `WorkingBodyA`, `WorkingBodyB` | Preprocess | Imprint 可修改的工作副本 |
| `FacePairs` | Intersect | BVH 宽相候选 |
| `Graph` | Intersect | `IntersectionGraph` |
| `Fragments` | Imprint | 分割后面片（FaceFragment） |
| `AVsB`, `BVsA` | Classify | 面相对对方 IN/OUT/ON |
| `Selection` | Select | CSG 选面结果 |
| `LastCompleted` | 各 Stage | 诊断 |

---

## 5. 各阶段规格

### 5.1 Preprocess

- 校验两操作数均为封闭 Solid、含 Face。
- 在 `Model` 内 **clone** A/B 子图 → `WorkingBodyA/B`（Imprint 会改拓扑，不得原地修改 Feature 持有体）。

### 5.2 Intersect

1. BVH 收集 `FacePairCandidate`。
2. 对每对调用 `IntersectorRegistry::Intersect`：
   - Plane–Plane → 线段（无限直线 ∩ 两面有限域）
   - Plane–Sphere → 圆弧
   - Sphere–Sphere → 圆弧
   - Plane–Cylinder / Sphere–Cylinder → 按现有解析结果扩展
3. 写入 `IntersectionGraph`；fuzzy 合并共点、共线段。

### 5.3 Imprint

1. 交点 → `SplitEdge(Edge*, t)`，更新 Vertex 链。
2. 交线 → 在 Face 上插入 CoEdge 链，产生 `FaceFragment`。
3. **M1（平面）**：2D 线 arrangement，半平面裁剪。
4. **M2/M3（弯曲）**：球/柱 UV pcurve；处理 seam / 极点。

Imprint 未完成前，Pipeline 在此 Stage 软失败并返回诊断。

### 5.4 Classify（纯通用，无 Recognize 分支）

```cpp
SolidClass ClassifyPointInBody(const Body& solid, const Point3d& p, double eps);
```

**唯一主路径** — 对所有操作数（盒、球、柱、NURBS、布尔结果体）相同算法：

1. **IN / OUT**：封闭壳 + **射线奇偶法**（或 winding number）。
   - 射线与 **每个 Face** 求交；交点计数判 inside/outside。
   - 求交按 **`Face.Surface->Kind()`** 分发（与 `IntersectorRegistry` 同构，但射线–曲面而非面–面）：
     - `Plane` → 射线–平面
     - `Sphere` / `Cylinder` → 解析射线–二次曲面
     - `Nurbs` → 射线–NURBS（牛顿/细分；**不用** display tessellation）
2. **ON**：点到 **所有 Face** 的最小有向距离 < `eps`（各曲面类型解析或 UV 投影距离）。
3. 对每个 `FaceFragment` 取代表点（质心 + 角点），相对 **对方 WorkingBody** 调用上述函数。

**明确禁止**

| 禁止 | 原因 |
|------|------|
| `RecognizeAxisAlignedBox` + `ClassifyPointInBox` | 体级特解；布尔后再布尔失效 |
| `RecognizeAnalyticSphere` + `ClassifyPointInSphere` | 同上 |
| 用 display mesh 做点在内判定 | 显示与拓扑精度解耦（ADR 0009） |

**与 NURBS 的关系**：NURBS 面只需在射线–曲面模块注册 `SurfaceKind::Nurbs` 求交器；**不**增加「识别成盒/球」分支。

**性能**：大模型可 BVH 加速「射线–Face 候选集」；仍是通用结构，非体类型特解。

### 5.5 Select

沿用 `SelectCsgFaces`（[2026-08-14 规格 §6](./2026-08-14-boolean-pipeline-design.md)）：

| Op | 保留 A | 保留 B |
|----|--------|--------|
| Union | OUT, ON | OUT, ON |
| Subtract (A−B) | OUT | IN（Build 时翻转） |
| Intersect | IN, ON | IN, ON |

### 5.6 Build

1. 在 `TargetModel` 创建 `Body` + `Shell`。
2. `TopologyCopy`：`Selection.FromA` / `FromB` 子图 deep copy。
3. Subtract 时翻转 B 侧 Face/CoEdge orientation。
4. 沿交线缝合 Partner（A 新边 ↔ B 新边）。
5. `ValidateBody`；失败返回 Build 诊断。
6. `BooleanResult.OutputBody` 指向新 Body；`Mode = General`。

---

## 6. 与旧特解的关系

| 旧路径 | 处置 |
|--------|------|
| `BoxBoolean` grid CSG | 删除；Box×Box 由 Plane Imprint + Classify 替代 |
| `SphereBoxBoolean` ⅛/⅞ 构型 | 删除；由 Plane–Sphere Imprint 替代 |
| `SphereSphereBoolean` | 删除；由 Sphere–Sphere Imprint 替代 |
| `PlanarBoolean` | 删除；棱柱面均为 Plane，走通用平面 Imprint |

特解中的 **几何洞见**（如 octant 过滤、trim 方向）可沉淀为 Imprint/Classify 单元测试或注释，不保留独立 evaluator。

---

## 7. 扩展新形体（示例：圆柱）

1. `CylinderSurface` + `Model::MakeCylinderSurface`（已有）。
2. 补全 `IntersectCylinderCylinder` 等 **曲面类型对**。
3. ImprintEngine 增加柱面 UV 分割。
4. **不需要** `CylinderBoxBoolean.cpp`。

---

## 8. 文件布局（目标）

```text
kernel/include/brep/bool/
  Pipeline.h
  Evaluator.h
  CompositeEvaluator.h          # 简化为 Pipeline 包装，或改名 PipelineBooleanEvaluator.h
  IntersectorRegistry.h
  IntersectionGraph.h             # 新建
  ImprintEngine.h                 # 新建
  SolidClassifier.h               # 新建
  BooleanBuilder.h                # 新建
  TopologyCopy.h                  # 新建
  FaceSelector.h
  Broadphase.h
  Classify.h                      # 遗留解析点测；Pipeline 不得 include（仅单测）
  Intersect*.h
  Result.h / Types.h / Context.h

kernel/src/bool/
  Pipeline.cpp
  CompositeEvaluator.cpp
  IntersectorRegistry.cpp
  IntersectionGraph.cpp
  ImprintEngine.cpp
  SolidClassifier.cpp
  BooleanBuilder.cpp
  TopologyCopy.cpp
  FaceSelector.cpp
  Broadphase.cpp
  Classify.cpp
  Intersect*.cpp
```

**删除**：`FastPath.*`、`BoxBoolean.*`、`SphereBoxBoolean.*`、`SphereSphereBoolean.*`、`PlanarBoolean.*`

---

## 9. 测试策略

| 类别 | 说明 |
|------|------|
| 迁移 | `TestBoxBoolean`、`TestSphereBoxBoolean`、`TestSphereCurvedBoolean` 保留几何用例，改 `mode=General` |
| 删除 | `TestBooleanPipeline` 中 FastPath / AnalyticPair 断言 |
| 新增 | `TestIntersectionGraph`、`TestImprintPlanar`、`TestSolidClassifier`、`TestBooleanChained` |
| 回归 | `ctest -R "boolean|BoxBoolean|SphereBox|SphereCurved"` |

---

## 10. 范围外

- NURBS 通用求交
- Sheet 裁剪 `SplitFeature`（可后续复用 Imprint）
- OCCT `IBooleanEvaluator` 插件（与自研 Pipeline 并列，见 ADR 0005）
- Stage 内部性能优化（允许，但不得恢复体对体 public API）

---

## 11. 参考

- 球–球拓扑难点：[2026-08-10 设计 §T3.6](./2026-08-10-analytic-sphere-brep-boolean-design.md)
- BVH 宽相：[2026-08-10 设计 §T4.4](./2026-08-10-analytic-sphere-brep-boolean-design.md)
- 实施步骤：[2026-08-24 实施计划](../plans/2026-08-24-boolean-general-pipeline-implementation.md)
