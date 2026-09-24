# NURBS 综合方案：融合主流 CAD 优点

**日期**：2026-08-24  
**状态**：已接受（总纲；分期交付见下链文档）  
**决策**：[ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)  
**分层落点**：[layering.md](../../architecture/layering.md) §7（存储 → core，显示 tessellate → mesh，求交 → bool，建模 UI → ADR 0011）  
**调研**：[CAD 功能调研](./2026-08-24-nurbs-display-cad-survey.md)  
**细分**：[显示设计](./2026-08-24-nurbs-display-viewer-design.md)、[显示计划](../plans/2026-08-24-nurbs-display-implementation.md)、[建模/UI 远期](./2026-08-24-nurbs-modeling-ui-roadmap.md)

---

## 1. 方案目标

在 **不自研完整 NX/CATIA 级造型系统** 的前提下，为 brepkernelstudy 设计一套 **可分期落地** 的 NURBS 能力，**各取所长**：

| 原则 | 说明 |
|------|------|
| **双表示** | 全行业共识：精确 B-Rep/NURBS ≠ 屏幕 mesh（§2.1） |
| **显示与制造/导出分离** | 屏幕、`.bks`、STL 三套质量可独立（Creo + SolidWorks） |
| **大模型友好** | 弦高随包围盒比例缩放 + 视图 LOD（CATIA + NX） |
| **造型可读** | Wireframe 用等参线 + 可选控制网（MicroStation + AutoCAD） |
| **机械用户习惯** | 质量滑块 Draft/Standard/Fine（SolidWorks） |
| **内核/Viewer 解耦** | 与 ADR 0002、0009 一致；Viewer 只消费 mesh |
| **先看后建后算** | Display → Feature/UI → 求交/布尔（ADR 0005/0008） |

**与 2026-08 实体修改需求（Copy/Move/抽壳）：** 不修订本总纲、不提前 ADR 0011。解析体 Duplicate 遇尚未实现的 `Nurbs` Kind 则失败；N1 有 `NurbsCurve/Surface` 之后，刚体变换只对控制点做仿射，仍不做 NURBS 求交/特征 UI。Copy 与 NURBS 显示分 PR。

---

## 2. 各软件优点 → 本方案采纳

### 2.1 架构层（全行业 + 本项目）

| 来源 | 优点 | 本方案 |
|------|------|--------|
| **全行业** | 精确几何与 GPU mesh 分离 | `Model` 存 NURBS；`Tessellate*` 产出 `TriangleMesh`/`EdgeMesh`；**禁止** mesh 写回 B-Rep |
| **SolidWorks** | Image Quality 滑块直观；tessellation 可选不入库 | Viewer **质量预设** + 默认 tessellation **仅缓存 `.bks`**，不写入 XL |
| **Creo** | Chord height + Angle + Step size 三参数；**显示与导出独立** | 统一 `NurbsDisplayQuality`（§4.1）；`ExportMeshOptions` 单独用于 STL |
| **CATIA** | Proportional sag（大零件粗、小特征细） | `ProportionalToBBox = true` 为默认；`sag = coeff × diag(bbox)` |
| **NX** | Part / View 两级 facet；Update Display 修复锯齿 | **Document 级**粗网格 + **相机距离**触发的 view refine（P1 Viewer） |
| **NX** | Align facets along edges | 可选 `AlignFacetsToEdges`（P2，三角边对齐拓扑边） |
| **MicroStation** | U/V rule lines 独立密度；控制网/曲面分开关 | **DisplayLayer** 位掩码（§4.2） |
| **AutoCAD** | CVSHOW/CVHIDE；Fit vs CV 两种样条创建 | 建模远期：**CV 模式**默认；Fit 点插值可选（路线图 P2） |
| **AutoCAD** | ISOLINES、DISPSILH（轮廓线） | 等参线 + 解析面 silhouette 复用现有 `ExtractEdges` 逻辑 |
| **CATIA** | 等参线参与曲率分析可视化 | 远期 P4：分析网格可基于等参线 + 更细 sag |

### 2.2 明确不照搬（控制范围）

| 软件做法 | 不照搬原因 | 本方案替代 |
|----------|------------|------------|
| SW 默认把 tessellation **存入零件文件** | XL 体积膨胀、与学习型内核「单一 B-Rep 真相」冲突 | `.bks` + 内存 LRU |
| CATIA 全功能 FreeStyle / G2 匹配 | 人力与求交/Trim 未就绪 | 路线图 P2 仅单曲面 CV 编辑 |
| NX 完整 Studio 特征集 | 同上 | P3 Loft/Sweep 占位 |
| 六软件六种 UI 入口 | 用户困惑 | **一个**「显示质量」+ **一个**「NURBS 显示层」对话框 |

---

## 3. 统一架构

```text
                    ┌─────────────────────────────────────┐
                    │         NurbsDisplayProfile         │
                    │  (质量 + 层 + Part/View 策略)        │
                    └─────────────────┬───────────────────┘
                                      │
     ┌────────────────────────────────┼────────────────────────────────┐
     │ Kernel                         │ Viewer (apps/viewer)           │
     │  brep_core: NurbsCurve/Surface │  Quality 预设 UI               │
     │  brep_mesh: Tessellate/Iso     │  Layer  toggles                │
     │  （目标拆库，见 layering.md）   │  Regenerate display            │
     │       │                        │       │                        │
     │       ▼                        │       ▼                        │
     │  NurbsDisplayQuality           │  api/Mesh → Vulkan             │
     │  TessellateFace / ExtractEdges │  BksCache(key=geom+profile)    │
     │  ExtractControlNet             │                                │
     └────────────────────────────────┴────────────────────────────────┘
                                      │
                    ┌─────────────────┴───────────────────┐
                    │ 远期：FeatureTree + EditTool (P1–P2) │
                    │ 更后：Trim/Sew/Boolean (ADR 0008+)   │
                    └─────────────────────────────────────┘
```

---

## 4. 核心类型（建议）

### 4.1 `NurbsDisplayQuality` — 融合 SW / CATIA / Creo / NX

```cpp
struct NurbsDisplayQuality {
  // Creo + CATIA + 现有 TessellationOptions
  double LinearDeflection{0.0};      // 0 → 自动（ProportionalToBBox）
  double AngularDeflection{15.0 * kDegToRad};
  double MaxEdgeLength{0.0};         // Creo Step Size；0 = 不限制

  // CATIA
  bool ProportionalToBBox{true};
  double BboxSagCoeff{0.001};        // sag ≈ coeff * diag(bbox)

  // 段数护栏（现有 Min/Max USegments）
  int MinUSegments{8};
  int MaxUSegments{128};
  int MinVSegments{8};
  int MaxVSegments{64};

  // NX：View 级 refine
  enum class FacetScale { Part, View };
  FacetScale Scale{FacetScale::Part};
  double ViewRefineFactor{0.5};    // 放大时 sag *= factor

  // NX（可选 P2）
  bool AlignFacetsToEdges{false};

  [[nodiscard]] static NurbsDisplayQuality Draft();
  [[nodiscard]] static NurbsDisplayQuality Standard();
  [[nodiscard]] static NurbsDisplayQuality Fine();   // 对标 SW High / NX Ultra Fine
};
```

**预设映射（用户可见）**

| 预设 | 对标 | 典型用途 |
|------|------|----------|
| **Draft** | SW Low / NX Standard | 大装配浏览 |
| **Standard** | SW Medium | 默认编辑 |
| **Fine** | SW High / NX Fine | 特写、截图 |
| **Ultra**（可选） | NX Ultra Fine | 曲率分析前 P4 |

### 4.2 `NurbsDisplayLayers` — 融合 MicroStation + AutoCAD

```cpp
enum class NurbsDisplayLayer : std::uint32_t {
  Shaded       = 1 << 0,  // 默认开
  Boundary     = 1 << 1,  // Face 边界 / ExtractEdges
  IsoLines     = 1 << 2,  // MicroStation U/V rules
  ControlNet   = 1 << 3,  // AutoCAD CVSHOW / MS Surface Polygon
  Silhouette   = 1 << 4,  // AutoCAD DISPSILH；解析面 + NURBS 轮廓
};

struct IsoLineOptions {
  int CountU{4};   // MicroStation U Rules 默认
  int CountV{4};
};

struct NurbsDisplayProfile {
  NurbsDisplayQuality Quality;
  NurbsDisplayLayer Layers{NurbsDisplayLayer::Shaded |
                           NurbsDisplayLayer::Boundary};
  IsoLineOptions IsoLines;
};
```

**模式组合（对标 CAD 显示模式矩阵）**

| 用户模式 | Layers |
|----------|--------|
| Shaded | Shaded + Boundary |
| Shaded + Edges | + Boundary（加粗） |
| Wireframe | Boundary + IsoLines |
| 造型编辑（远期 P2） | + ControlNet |
| 工程图式（远期） | + Silhouette，HLR 另项 |

### 4.3 `ExportMeshOptions` — 融合 Creo 导出（与屏幕分离）

```cpp
struct ExportMeshOptions {
  double ChordHeight{0.0};   // 0 = 自动最小合法值（Creo）
  double AngleControl{1.0};
  double MaxStepSize{0.0};
};
```

STL/打印走 `ExportMeshOptions`，**不修改** `NurbsDisplayProfile`，避免「屏幕好看但 STL 粗糙」或反之。

### 4.4 曲线创建语义 — 融合 AutoCAD（远期 P2）

| 模式 | 来源 | 用途 |
|------|------|------|
| **ControlVertices (CV)** | AutoCAD SPLMETHOD=CV | 默认；编辑友好 |
| **FitPoints** | AutoCAD Fit | 插值点过曲线 |
| **ThroughCurveMesh** 等 | NX | 路线图 P3，不在首期 |

---

## 5. 管线行为（统一规则）

### 5.1 Tessellation 算法（Kernel）

1. **曲线**：自适应弦高 + 角度（Creo + 现有 `TessellationOptions` 思路）。
2. **未裁剪曲面**：UV adaptive 细分直至满足 `NurbsDisplayQuality`。
3. **裁剪 NURBS 面**：参数域 **CDT**（已有 Plane/Sphere 管线）→ 映射 3D。
4. **法向**：曲面偏导解析计算（优于纯顶点平均，对标 NX Fine 观感）。

### 5.2 缓存键（SolidWorks 不入库 + 性能）

```text
BksCacheKey = hash(BodyGuid, FaceTopologyRevision, NurbsDisplayProfile, FacetScaleContext)
```

- `FaceTopologyRevision`：CV/节点变更时 bump（远期 Feature Rebuild）。
- **View refine**：仅当 `FacetScale == View` 且相机变化超过阈值时重算；结果可短 TTL 内存缓存，不写 XL。

### 5.3 Regenerate Display（NX）

Viewer 提供 **「更新显示」**（对标 NX Update Display）：

- 不触发 `Part::Regenerate`（特征/几何不变）
- 仅 invalidate tessellation 缓存并按当前 `NurbsDisplayProfile` 重算

### 5.4 交互编辑（远期 P2，AutoCAD + NX）

| 阶段 | 行为 |
|------|------|
| 拖动 CV | 仅更新 `Model` + **预览 mesh**（低 Quality Draft） |
| 鼠标释放 | `MarkDirty` → `Rebuild` → Fine 缓存 |

避免 SW/NX 式「每次拖动全量 Fine tessellation」卡顿。

---

## 6. 分期交付（与现有文档对齐）

| 阶段 | 内容 | 采纳的 CAD 优点 | 文档 |
|------|------|-----------------|------|
| **N1** | de Boor + 曲线 mesh | Creo 弦高曲线 | [显示计划](../plans/2026-08-24-nurbs-display-implementation.md) |
| **N2** | Uniform 曲面 | — | 同上 |
| **N3** | `NurbsDisplayQuality` 自适应 + Draft/Standard/Fine | CATIA sag + Creo 三角 + SW 预设 | 同上 + **本文 §4.1** |
| **N4** | 裁剪面 CDT | — | 同上 |
| **N5** | `DisplayLayers` + IsoLines | MicroStation + AutoCAD ISOLINES | 同上 + **本文 §4.2** |
| **N6** | ControlNet + View LOD + Regenerate Display | AutoCAD CVSHOW + NX View + Update | 同上 |
| **P1** | NurbsSurfaceFeature | SW 参数化习惯（ParameterStore） | [建模路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md) |
| **P2** | CV 编辑、Fit/CV 模式 | AutoCAD SPLINE + 3DEDITBAR | 同上 |
| **P3** | Loft/Sweep/Trim | NX 造型（缩小版） | 同上 |
| **P4** | 曲率梳 / 斑马线 | CATIA 分析 | 同上 |
| **后** | NURBS 求交/布尔 | — | ADR 0008+ |

---

## 7. Viewer UI 建议（一个入口）

**Preferences → 显示 → NURBS**（对标多软件分散设置，合并为一处）

| 控件 | 对应 |
|------|------|
| 质量预设下拉 | Draft / Standard / Fine / Ultra |
| 高级… | LinearDeflection、AngularDeflection、MaxEdgeLength、ProportionalToBBox |
| Facet 范围 | Part / View（NX） |
| 显示层 | ☑ 着色 ☑ 边界 ☑ 等参线 ☐ 控制网 ☐ 轮廓 |
| U/V 等参线数 | 4 / 4（MicroStation 默认） |
| [更新显示] | NX Regenerate |
| 导出 STL… | 独立 Export 对话框（Creo chord/angle/step） |

模板/文档默认值：**Standard + Part scale + Shaded+Boundary**。

---

## 8. 文件与模块（目标）

| 模块 | 路径 | 职责 |
|------|------|------|
| `NurbsDisplayProfile.h` | `kernel/include/brep/` | Quality + Layers + IsoLines |
| `NurbsEval.h/.cpp` | kernel | 求值 |
| `MeshNurbs.cpp` | kernel | tessellation / isolines / control net |
| `NurbsDisplay.cpp` | kernel | `BuildDisplayMeshes(Body, NurbsDisplayProfile)` 统一入口 |
| Viewer 设置 | `apps/viewer/ui/` | 预设与层开关 |
| `.bks` | `brep_io` | profile 哈希进缓存键 |

`BuildDisplayMeshes` 返回 `{ TriangleMesh shaded; EdgeMesh edges; EdgeMesh isolines; EdgeMesh controlNet; }`，Viewer 按 `Layers` 选择性上传 GPU。

---

## 9. 验收标准（综合）

| 项 | 标准 |
|----|------|
| 双表示 | 改 Quality 不改变 `Model` 内 NURBS 控制点 |
| Draft vs Fine | 同曲面 Fine 三角数 > Draft，视觉更光滑（SW 球体 file 类比） |
| Proportional | 大 bbox 体在相同 coeff 下三角数少于小 bbox 体（CATIA） |
| View LOD | 放大后 sag 减小、mesh 更细；缩小可复用 Part 级（NX） |
| IsoLines | U/V=4 时 wireframe 可见 4×4 规则线（MicroStation） |
| ControlNet | 开关仅影响 overlay，不影响 shaded（AutoCAD） |
| Export ≠ Display | 同零件 Display=Draft、Export=Fine，STL 更密（Creo） |
| Regenerate Display | 按钮只刷新 mesh，不触发 Feature Rebuild（NX） |

---

## 10. 文档关系

```text
本文（总纲：Best-of-breed 融合）
 ├── CAD 调研（证据来源）
 ├── ADR 0009（近期范围：Display）
 ├── 显示设计（Kernel/Viewer 细节，应吸收 §4 类型）
 ├── 显示计划（N1–N6 任务）
 └── 建模/UI 路线图（P1–P4 远期）
```

后续实施时：**以本文 §4–§5 为 API 契约**，显示设计与计划中的 `TessellationOptions` 扩展逐步迁移为 `NurbsDisplayProfile`。

---

## 11. 修订记录

| 日期 | 说明 |
|------|------|
| 2026-08-24 | 初版：六软件优点映射 + 统一类型 + 分期 |
