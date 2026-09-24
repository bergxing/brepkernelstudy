# NURBS 曲线/曲面显示：主流 CAD 调研

**日期**：2026-08-24  
**状态**：调研归档  
**用途**：为 brepkernelstudy NURBS **显示**（非布尔求交）提供行业参照  
**关联**：[Viewer 设计规格](./2026-08-24-nurbs-display-viewer-design.md)、[实施计划](../plans/2026-08-24-nurbs-display-implementation.md)、[ADR 0009](../../architecture/adr/0009-nurbs-display-scope.md)、**[综合方案（Best-of-breed）](./2026-08-24-nurbs-unified-strategy.md)**

---

## 1. 核心结论

所有成熟 CAD 均采用 **双表示**：

| 层 | 内容 | 用途 |
|----|------|------|
| **精确几何** | NURBS / B-Rep：控制点、节点、权重、参数域 | 建模、求交、布尔、CAM |
| **显示网格** | Tessellation：三角面片、折线近似 | GPU 渲染、隐藏线、STL 导出 |

GPU **不能**直接绘制 NURBS；屏幕上「光滑曲面」均为 **离散近似**。用户调节的显示质量，本质是在调节 **弦高（chord height / sag）**、**法向夹角**、**最大边长** 等 tessellation 参数。

本仓库 `TessellationOptions`（`LinearDeflection`、`AngularDeflection`）与 CATIA sag、SolidWorks deviation、Creo chord height 属同一概念族。

---

## 2. 显示模式矩阵（行业惯例）

| 模式 | NURBS 曲线 | NURBS 曲面 | 典型用途 |
|------|-----------|-----------|----------|
| **Wireframe** | 曲线或折线近似 | 边界 + **U/V 等参线** | 建模、检拓扑 |
| **Shaded（Facet）** | 管状 mesh | **三角网格 + 法向** | 日常浏览 |
| **HLR / Hidden Line** | 可见边 | 可见边 + 轮廓线（Silhouette） | 工程图式沟通 |
| **Control net** | 控制折线 | 控制网格 | 造型编辑、调试 |
| **Analysis overlay** | 曲率梳 | 曲率云图、斑马线 | 质量检查 |

---

## 3. 质量参数对照

| 概念 | AutoCAD | CATIA | SolidWorks | Creo / NX | brepkernelstudy |
|------|---------|-------|------------|-----------|-----------------|
| 弦高 / sag | `FACETRES` 等 | Fixed / Proportional sag | Deviation（Image Quality） | Chord Height | `LinearDeflection` |
| 法向角度 | — | — | Visualize Max angle | Angle Control | `AngularDeflection` |
| 最大边长 | — | — | Max length | Step Size | 可扩展 segment cap |
| 等参线密度 | `ISOLINES` | Isoparametrics | — | — | 可新增 `IsoLineCount` |
| 按视图缩放 | — | — | — | NX Facet Scale = View | 可 view-dependent LOD |
| 控制网显示 | `CVSHOW` / `CVHIDE` | Control Points | — | — | 建模/debug 开关 |
| Tessellation 持久化 | 部分写入 DWG | 会话/设置 | 可选存入零件文件 | LOD / 导出独立 | 建议 `.bks` 缓存，不写回 B-Rep |

---

## 4. 各软件功能摘要

### 4.1 AutoCAD

**建模**

- `SPLINE`：NURBS 曲线；**Fit points**（插值）或 **Control Vertices**（控制顶点）；阶数 1–10。
- NURBS 曲面可编辑 CV（`3DEDITBAR`、拖动控制顶点）。

**显示**

| 功能 | 说明 |
|------|------|
| 控制顶点 | `CVSHOW` / `CVHIDE` |
| 等参线 | 系统变量 `ISOLINES` |
| 实体曲面网格 | `FACETRES` |
| 圆弧显示 | `VIEWRES` |
| 轮廓线 | `DISPSILH=1`（silhouette） |

**特点**：Wireframe 下靠 **边界 + isolines** 表达曲率；着色时用 facet。

**参考**：[SPLINE 命令](https://help.autodesk.com/cloudhelp/2026/ENU/AutoCAD-Core/files/GUID-5E7D51E2-1595-4E0C-85F8-2D7CBD166A08.htm)、[CVSHOW](https://help.autodesk.com/cloudhelp/2026/ENU/AutoCAD-Core/files/GUID-D2C0E238-992F-4DBD-926B-56466F73C8E4.htm)

---

### 4.2 MicroStation（Bentley）

**设置**：`File > Settings > File > 3D and B-spline`

**显示**

| 功能 | 说明 |
|------|------|
| Curve Polygon | B-spline 曲线控制折线，可显/隐 |
| Surface Polygon | B-spline 曲面控制网，可显/隐 |
| Iso Lines | `Surface/Solid Iso Lines`、`U Rules` / `V Rules` |
| Wireframe | 曲面 = 边界 + 等参线；平面仅边界 |

**特点**：**等参线密度** 是 wireframe 表达曲率的核心；控制网独立开关，适合曲面建模阶段。

**参考**：[3D and B-splines Dialog](https://docs.bentley.com/LiveContent/web/MicroStation-v2026/Help/en/topics/122986/GUID-A6E19686-8D87-A5C6-EE5E-E8A3E0E11EB7.html)、[Display of Solids and Surfaces](https://docs.bentley.com/LiveContent/web/MicroStation-v2026/Help/en/topics/122997/GUID-11C3F891-A705-9041-A6B2-6401B90D7C68.html)

---

### 4.3 Siemens NX / UG

**建模**：Studio Spline、Through Curve Mesh、NURBS 曲面为造型主力。

**显示**（`Preferences > Visualization`）

| 功能 | 说明 |
|------|------|
| Facet 精度 | Standard → Fine → Ultra Fine |
| Facet Scale | **Part** vs **View**：View 按当前缩放重算 facet |
| Regenerate / Update Display | 重算 facet，修复锯齿 |
| Align Facets along Edges | 三角对齐边（质量↑，性能↓） |
| 边显示 | 光滑边、隐藏边、Silhouette（柱/球/环面） |

**特点**：**Part/View 两级 facet**——大装配粗网格，特写/渲染细网格。

**参考**：[NX Visualization Faceting](https://community.sw.siemens.com/s/question/0D54O000061wwn1SAA/regenerating-model-before-rendering)、[Create and Control NURBS](https://community.sw.siemens.com/s/article/37712-create-and-control-nurbs-curves-surfaces)

---

### 4.4 SolidWorks

**显示**

| 功能 | 说明 |
|------|------|
| Image Quality | 文档属性滑块；控制 **Deviation（弦高）** |
| Save tessellation with part | 是否将显示网格存入文件（影响体积与打开速度） |
| Visualize 导入 | Tolerance、Max length、Max angle 三参数 |
| 工程图 | Improve curve quality at higher settings |

**特点**：球/环面 file size 可因 tessellation 差 **10–20×**；精确 B-Rep 与显示网格分离管理。

**参考**：[Document Properties - Image Quality](https://help.solidworks.com/2024/english/sldworks/HIDD_OPTIONS_IMAGE_QUALITY_DISPLAY.htm)、[Visualize Import Tessellation](https://help.solidworks.com/2024/english/Visualize/r_import_settings_dialog_box.htm)

---

### 4.5 CATIA

**显示**（`Settings > Display > Performance`）

| 功能 | 说明 |
|------|------|
| 3D Accuracy / Sag | Fixed sag 或 **Proportional to element size** |
| Curve accuracy ratio | 曲线折线精度相对 3D 精度 |
| Isoparametrics | 可生成 U/V 等参线；参与曲率分析可视化 |
| Control Points | 与曲率分析叠加显示 |

**特点**：大物体自动用更粗网格（proportional sag）；造型阶段 **等参线 + 控制点** 与三角 mesh 并存。

**参考**：[Performance / Tessellation](https://catiahelp.azurewebsites.net/English/SettingsMap/settingsdisplay-c-Performance.htm)

---

### 4.6 Creo Parametric（PTC）

**显示 / 导出**

| 功能 | 说明 |
|------|------|
| Chord Height | 弦高，tessellation 主控 |
| Angle Control | 相邻三角法向最大夹角 |
| Step Size | 三角边长上限 |
| Model Display vs Export | 屏幕 shading 与 STL **可独立设置** |

**特点**：导出 STL 时 chord height = 0 表示自动取模型允许最小值。

**参考**：[Controlling the Quality of Export](https://support.ptc.com/help/creo/creo_pma/r13/usascii/data_exchange/interface/Controlling_the_Quality_of_Export.html)

---

## 5. 对 brepkernelstudy 的启示（摘要）

1. **显示层优先**：先实现 NURBS 曲线折线化 + 曲面 adaptive tessellation + 可选等参线；不要求 NURBS 布尔。
2. **复用 `TessellationOptions`**：弦高 + 角度 + segment 上下限，与 CATIA/SW/Creo 对齐。
3. **Wireframe 辅助线**：等参线、控制网作为 **可选 display layer**，默认关闭或建模模式开启。
4. **缓存策略**：tessellation 进 `.bks` 或 Viewer 内存缓存；**不写回**精确 B-Rep（对齐 SolidWorks 可选存 tessellation 的思路，但更轻）。
5. **与布尔路线解耦**：NURBS 求交/Imprint 见 [ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md)，**晚于**解析曲面 Pipeline。
6. **建模/UI 远期**：特征树与自由曲面交互见 [建模 UI 路线图](./2026-08-24-nurbs-modeling-ui-roadmap.md)（占位，未排期）。
7. **综合方案**：各软件优点融合见 [NURBS 综合方案](./2026-08-24-nurbs-unified-strategy.md)。

---

## 6. 参考索引

| 软件 | 文档 / 入口 |
|------|-------------|
| AutoCAD | SPLINE、CVSHOW、ISOLINES、FACETRES |
| MicroStation | 3D and B-splines Dialog |
| NX / UG | Preferences > Visualization > Faceting |
| SolidWorks | Document Properties > Image Quality |
| CATIA | Settings > Display > Performance |
| Creo | Export Chord Height / Angle Control |
