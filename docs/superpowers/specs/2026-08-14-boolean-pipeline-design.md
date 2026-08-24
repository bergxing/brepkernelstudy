# 布尔通用 Pipeline 设计规格

**日期**：2026-08-14  
**状态**：草案（P0 骨架已落地）  
**关联**：[ADR 0006](../../architecture/adr/0006-boolean-pipeline-architecture.md)、[2026-08-10 布尔设计](./2026-08-10-analytic-sphere-brep-boolean-design.md)

---

## 1. 目标

| 目标 | 说明 |
|------|------|
| **对称 CSG** | `A−B` 只由操作数顺序决定；Box−Sphere 与 Sphere−Box 同一 Pipeline |
| **可扩展** | 新曲面 → 注册 `Intersector`；新体类型 → 无需新 `evaluate_*_*` |
| **特解可选** | AnalyticPair 快路径可插拔，失败或未覆盖时回退 Pipeline |
| **可观测** | 每 Stage 软失败带 `PipelineStage` + 诊断字符串 |
| **未来 Split** | Imprint 阶段可复用于「体被曲面裁剪」 |

---

## 2. 顶层结构

```text
BooleanFeature / Viewer
        │
        ▼
  IBooleanEvaluator  ◄─── 测试可注入 Fake
        │
        ▼
CompositeBooleanEvaluator
   ├─► AnalyticFastPathRegistry  (try in order, can_handle strict)
   └─► BooleanPipeline           (general path)
           │
           ├─ PreprocessStage
           ├─ IntersectStage      ──► IntersectorRegistry + FaceBvh
           ├─ ImprintStage        (P1 待实现)
           ├─ ClassifyStage       (P1 待实现)
           ├─ SelectStage         ──► face_selector (CSG 规则)
           └─ BuildStage           (P1 待实现)
```

---

## 3. 核心类型

### 3.1 `PipelineStage`

```cpp
enum class PipelineStage : std::uint8_t {
  Preprocess = 0,
  Intersect,
  Imprint,
  Classify,
  Select,
  Build,
};
```

### 3.2 `PipelineState`（阶段间共享）

| 字段 | 产出 Stage | 用途 |
|------|------------|------|
| `op`, `body_a`, `body_b`, `ctx`, `model` | 输入 | 运算与输出 Model |
| `face_pairs` | Intersect | BVH 宽相候选 |
| `probe` | Intersect | 解析求交统计 |
| `intersection_graph` | Intersect | 未来：交线/交点图 |
| `classifications` | Classify | 面相对对方实体的 IN/OUT/ON |
| `selected_faces` | Select | CSG 选面结果 |
| `last_completed` | 各 Stage | 调试 / 诊断 |

### 3.3 `IPipelineStage`

```cpp
struct PipelineStageResult {
  bool ok{false};
  PipelineStage stage{};
  std::string diagnostics;
};

class IPipelineStage {
  virtual PipelineStage id() const = 0;
  virtual PipelineStageResult run(PipelineState& state) = 0;
};
```

### 3.4 `BooleanPipeline`

- 构造时 `register_default_stages()` 按顺序注册六阶段。
- `evaluate(op, model, a, b, ctx)` → 填充 `PipelineState`，顺序 `run`，任一失败即返回 `BooleanResult`（`mode=General`，`diagnostics` 含阶段名）。

---

## 4. AnalyticFastPath

```cpp
class IAnalyticFastPath {
  virtual std::string_view name() const = 0;
  virtual bool can_handle(BooleanOp, const Body& a, const Body& b,
                          const BooleanContext&) const = 0;
  virtual BooleanResult evaluate(...) = 0;
};
```

**注册顺序（默认）**：

1. `BoxBoxFastPath` — `recognize_axis_aligned_box` × 2  
2. `PrismBoxFastPath` — 拉伸 prism × box  
3. `SphereBoxFastPath` — 球 × 盒；**Subtract 仅当 A 为球**（`can_handle` 严格）  
4. `SphereSphereFastPath` — 球 × 球  

**Composite 策略**：

```text
for fp in registry:
  if fp.can_handle(...):
    return fp.evaluate(...)   // 成功或失败均返回，不掩盖特解诊断
return pipeline.evaluate(...)
```

Box−Sphere Subtract：`SphereBoxFastPath::can_handle` 为 **false** → 进入 Pipeline（P1 实现后产出实体）。

---

## 5. IntersectorRegistry

按 **无序曲面类型对** 注册（内部规范化 `min(kind), max(kind)`）：

| Key | 函数 |
|-----|------|
| Plane–Plane | `intersect_plane_plane` |
| Plane–Sphere | `intersect_plane_sphere` |
| Sphere–Sphere | `intersect_sphere_sphere` |
| Plane–Cylinder | `intersect_plane_cylinder` |
| Sphere–Cylinder | `intersect_sphere_cylinder` |

`IntersectStage` 对每个 `FacePairCandidate` 调用 registry；统计 `nonempty / empty / unsupported`（与现有 `BroadphaseProbe` 对齐）。

**不注册** Box–Sphere：盒面均为 Plane，球面为 Sphere，已覆盖。

---

## 6. CSG 选面规则（SelectStage）

对操作体 **A** 的每个面片 `f`（相对 **B** 分类）：

| Op | 保留 A 的面 | 保留 B 的面 |
|----|-------------|-------------|
| Union | OUT, ON | OUT, ON |
| Subtract (A−B) | OUT | IN（法向取反） |
| Intersect | IN, ON | IN, ON |

`FaceSelector.hpp` 提供纯函数 `select_csg_faces(...)`，不依赖印记后拓扑；P1 Imprint 完成后输入改为分割后面片。

---

## 7. 分期交付

### P0（本提交）— 骨架

- [x] ADR 0006 + 本文档  
- [x] `PipelineState` / `IPipelineStage` / 六阶段 stub  
- [x] `IntersectorRegistry` + `IntersectStage` 接宽相  
- [x] `AnalyticFastPathRegistry` + `CompositeBooleanEvaluator`  
- [x] `face_selector` CSG 规则（逻辑层）  
- [x] `brep_test_boolean_pipeline`  

### P1 — Box↔Sphere 通用闭环

- [ ] Imprint：沿交线分割边/面  
- [ ] Classify：面心/采样点相对对方实体  
- [ ] Build：缝 Shell、`validate_body`  
- [ ] Box−Sphere Subtract 端到端测试  

### P2 — 扩展

- [ ] Cylinder 体通用布尔  
- [ ] 特解与 Pipeline 结果对照回归  
- [ ] SplitFeature（曲面裁剪）复用 Imprint  

---

## 8. 文件布局

```text
kernel/include/brep/bool/
  Pipeline.h
  IntersectorRegistry.hpp
  FastPath.hpp
  CompositeEvaluator.hpp
  FaceSelector.hpp

kernel/src/bool/
  pipeline.cpp
  IntersectorRegistry.cpp
  FastPath.cpp
  CompositeEvaluator.cpp
  FaceSelector.cpp
```

`EvaluatorStub.cpp` 保留 `make_stub_boolean_evaluator()`；`make_default_boolean_evaluator()` 移至 `CompositeEvaluator.cpp`。

---

## 9. 测试

| 用例 | 断言 |
|------|------|
| `PipelineRunsStagesInOrder` | Preprocess+Intersect 成功；Imprint 软失败且诊断含 `Imprint` |
| `CompositeUsesBoxBoxFastPath` | 两盒 Union 成功，`mode=AnalyticPair` |
| `CompositeBoxMinusSphereUsesPipeline` | 盒−球 Subtract 特解不拦截；Pipeline 诊断含阶段信息 |
| `FaceSelectorSubtract` | 纯逻辑：A OUT 保留、B IN 保留 |

---

## 10. 与 Viewer 的关系

- 选择顺序 → `target`=A、`tool`=B（Subtract 已按 pick order 修复）。  
- Pipeline 不关心 pick 语义，只接收 `(op, body_a, body_b)`。  
- 失败诊断经 `Part::last_regen_detail()` 展示。
