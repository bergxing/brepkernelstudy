# Move 命令 — 交互流程

**日期：** 2026-08-27  
**状态：** 实现（G3）  
**关联：** [Adapter 治理 §8.1](../../architecture/viewer-adapter-governance.md)、[治理排期 G3](../plans/2026-08-27-viewer-adapter-governance-implementation.md)、[CopyTool](../../../apps/viewer/commands/tools/CopyTool.cpp)

**Goal：** Viewer 里用与 Copy **同一套拾取** 把选中体素平移到新位置：源体不保留副本，提交走已有 `TransformBody`，**不**给 `ISceneService` 加新方法。

---

## 1. 主流软件怎么做（取舍）

| 软件 | 命令 | 用户在点什么 | 结果 | 本方案是否照搬 |
|------|------|--------------|------|----------------|
| **AutoCAD** | `MOVE`（别名 `M`） | 先选对象（或预选）→ **基点** → **第二点**；ESC 取消 | 源对象移到新位置，无副本 | **交互骨架**：两步点选 + 预选/后选 |
| **DraftSight / ZWCAD / GstarCAD** | 同 AutoCAD | 同上 | 同上 | 同上 |
| **SolidWorks** | Move/Copy Bodies | 选实体 → 对话框：平移增量 **或** 两点；可选 3D 三轴手柄 | 可勾选 Copy | **不做** 对话框与三轴手柄（v1） |
| **Inventor** | Move Bodies | 选实体 → 拖手柄或输入偏移 | 就地 | **不做** 手柄 |
| **Fusion 360** | Move / Copy | 选对象 → 出现 triad；可改成点到点；Copy 是同一工具上的勾选 | 移动或复制 | **拆成两条命令**（已有 Copy）；Move 只用点到点 |
| **NX** | Move Object | 选对象 → 动态箭头 / 两点 / 增量 | 就地 | **不做** 动态箭头 |
| **Rhino** | Move | AutoCAD 同款：基点 → 目标点 | 就地 | 与 AutoCAD 一并作为正本 |

**本仓库 v1 选定：AutoCAD / Rhino 的「基点 → 目标点」平移。**

不选 Fusion/SW 三轴手柄：本 Viewer 没有变换 gizmo 基础设施，Copy 已经是两点拾取；Move 与 Copy 只差「是否留下源体」，再做一套手柄是重复 UI。  
不选「输入 ΔX/ΔY/ΔZ 对话框」：状态栏两点即可算出同一 `RigidTransform`；数值输入可以后加，不阻塞 G3。  
不选旋转 / 缩放：`TransformBody` 目前只允许 `IsTranslation()`（G1）。

Copy 与 Move 对照（治理 §8.1）：

| | Copy（已有） | Move（本命令） |
|--|-------------|---------------|
| 拾取 | 选对象 → 基点 → 目标点 | **相同** |
| 内核 | 体素 `AddPrimitive`；结果体 `DuplicateBody` | 体素 `TransformBody` |
| 源体 | 保留 | **不保留副本**（对象 Guid 不变） |
| 结果体（拉伸/融合/CopiedBody） | 可复制 | **v1 整单拒绝**（就地 Placement 见 G6） |

---

## 2. 产品范围

### 做

- 命令 `edit.move`：预选或命令内选择 → 基点 → 目标点 → 就地平移。
- AccuSnap：与 Copy / CreateBox 相同（表面、端点、地面）。
- 实时预览：基点十字 + 位移线段 + 把源体已有边网格平移 `T`（不按盒子/球各画一套线框）。
- 提交：`ISceneService::TransformBody`；特征历史可撤销。
- 多选：多个立方体/球体用 **同一个** `T` 一起移动。
- i18n：`QCoreApplication::translate("MoveTool", …)` + `xcad_zh_CN.ts`。

### 不做（v1）

- 旋转、缩放、镜像（`det < 0`）。
- 三维拖动手柄 / 属性栏输入增量。
- 拉伸、融合、CopiedBody 等结果体就地移动（G6：特征 `Placement`）。
- 部分成功：选中里只要有一个不支持的对象，**整单拒绝**，不把盒子挪走而留下融合体。
- 新的 Adapter 虚函数（禁止 `MoveBox` / `move_body`）。

---

## 3. 命令

| 项 | 值 |
|----|-----|
| id | `edit.move` |
| 菜单 | Edit → Move… |
| 快捷键 | `Ctrl+Shift+M`（与 Copy 的 `Ctrl+Shift+C` 并列；不占 `Ctrl+C` 剪贴板，也不占 Modeling 菜单的 M） |
| 工具栏 | Copy 右侧 |
| 右键菜单 | Copy 下方 |
| 类型 | `CommandKind::Interactive` → `MoveTool` |
| 面板搜索 | 标题 `Move` |

菜单英文源文 / 中文：

- `&Move…` / `移动(&M)…`
- Tooltip：`Move selected objects: base point → place point` / `移动选中对象：基点 → 放置点`

---

## 4. 交互逻辑

对齐 `CopyTool`：`allows_viewport_selection()` 仅在步骤 0 为 true；AccuSnap；`SetPreviewEdges`；ESC 取消。

### 4.1 状态机

```text
                    已有合法预选                          拾取基点                 拾取目标点
[Start] ──有盒子/球──► [Base] ──────────────────────► [Place] ──── commit ──► 结束
   │                      │                             │
   │ 无预选                │ ESC                         │ ESC / 基点重合
   ▼                      ▼                             ▼
[Select] ──Space──► [Base]                           留在 Place，提示换点
   │
   │ 选中含拉伸/融合/草图 或 空选后 Space
   └── 提示「移动目前支持立方体和球体」/「请先选择…」，仍停在 Select
```

### 4.2 步骤与提示

| 步骤 | `m_step` | 视口 | 状态栏（英文源文） |
|------|----------|------|-------------------|
| 选择 | 0 | 单击 / 框选 / Ctrl+单击 | `Move: click/drag/Ctrl+click to select, Space to confirm (ESC cancel)` |
| 基点 | 1 | 左键吸附拾取 | `Move: pick base point (ESC cancel)` |
| 目标 | 2 | 左键；移动时预览线框 | `Move: pick destination (ESC cancel)` |

取消：`Cancelled move`。  
未命中：`Missed surface/ground — try another angle`（与 Copy 同句，便于翻译复用语义）。  
零位移：`Base and destination are the same point — pick farther`。

### 4.3 选择规则

收集当前 `SelectedTag`：

1. `SpecFor(featureGuid, bodyGuid)` **有值**（Box / Sphere）→ 记入移动列表（Guid + spec，spec 只用于预览）。
2. 否则（融合、拉伸、CopiedBody、草图、无 Body）→ 计为不支持。

进入基点步骤的条件：**列表非空，且不支持计数为 0**。

| 预选 | 行为 |
|------|------|
| 一个或多个盒子/球 | 直接进入基点 |
| 空 | 停在选择，提示选完按空格 |
| 仅融合/拉伸 | 提示「移动目前支持立方体和球体」，不进入基点 |
| 盒子 + 融合 | 同上（整单拒绝） |

### 4.4 提交与撤销

```text
T.Translation = place - base
对列表中每个 bodyGuid：
    TransformBody(bodyGuid, T)     // 失败则撤销已成功的步，整单失败
sync_part_bodies                   // 原 Guid 实体更新网格，不新建 ECS
DocumentHistory：undo_feature(n) / redo_feature(n)
```

`TransformBody` 在内核写入 `TxKind::TransformBody`，这样 Viewer 的 `undo_feature` 撤的是位移而不是误删特征。

失败且未移动任何对象：`Failed to move the selected object(s)`。  
成功：`Moved %1 object(s)`。

---

## 5. 与 Copy 的操作对照（给用户）

典型路径（与 AutoCAD `COPY` / `MOVE` 相同）：

1. 点选立方体或球体（可多选）。
2. 编辑 → 移动，或 `Ctrl+Shift+M`，或工具栏 Move，或右键 Move。
3. 点 **基点**（常吸附到角点、球心、已有端点）。
4. 点 **目标点**；移动鼠标时可看到线框跟着走。
5. 对象出现在新位置；`Ctrl+Z` 回到移动前。

Copy 在第 4 步之后会 **多出一个** 对象；Move **不会**。

---

## 6. 验收

- 移动盒子：边长不变，角点平移 `T`；Guid 不变。
- 移动球体：半径不变，球心平移 `T`。
- 多选两个盒子：共用一个 `T`。
- 选中融合体再 Move：提示不支持，模型不变。
- 盒子+融合混合选择：提示不支持，盒子也不动。
- Undo / Redo：位置还原 / 再应用；撤销不会删掉盒子。
- ESC：预览消失，位置不变。

---

## 7. 以后（非本命令）

| 项 | 阶段 |
|----|------|
| 拉伸/融合/CopiedBody 就地 Move | G6 `Placement` |
| 输入 ΔX/ΔY/ΔZ 或锁定轴 | 可选，不改 Adapter |
| 旋转 / 三轴手柄 | 另开变换 gizmo |
| 镜像 | Sense + `det < 0`，另命令 |
