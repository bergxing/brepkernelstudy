# 布尔两体命令 — 交互流程

**日期：** 2026-09-17  
**状态：** 实现  
**关联：** [`BooleanTwoBodyTool`](../../../apps/viewer/commands/tools/BooleanTwoBodyTool.cpp)、[`BooleanExecute`](../../../apps/viewer/commands/tools/BooleanExecute.cpp)、[2026-08-10 布尔设计](./2026-08-10-analytic-sphere-brep-boolean-design.md) T2.0.6、[实体造型特征](./2026-08-31-solid-modeling-features-design.md) §2.2、[Adapter 治理 §8.3](../../architecture/viewer-adapter-governance.md)

**Goal：** `boolean.union` / `boolean.subtract` / `boolean.intersect` 共用差集那一套两体拾取，不再要求「必须先恰好选中两个再点命令」。

---

## 1. 产品范围

### 做

- 三条命令都是 `CommandKind::Interactive`，共用 `BooleanTwoBodyTool(BooleanOp)`。
- 三种进入方式（与差集相同）：
  1. **先命令后选：** 点 Ribbon / 菜单 / 右键 → 点目标体 → 点工具体 → Enter 或右键确定。
  2. **预选目标：** 先选一个体再发命令 → 只再点工具体 → 确认。
  3. **预选两体：** 已按选择顺序选了两个不同体 → 发命令（或右键对应项）立即执行。
- 第二步拾取跳过已选目标（重叠时可点穿）。
- ESC 取消；确认前右键菜单：确定 / 取消。
- 提交仍走 `ISceneService::AddBoolean` + `ExecuteBoolean`（操作数 Guid，不缓存 `Body*`）。
- i18n：`QCoreApplication::translate("BooleanTwoBodyTool", …)` + `xcad_zh_CN.ts`。

### 不做

- 多于两个操作数的链式布尔（仍只取选择顺序的前两个）。
- 新的 Adapter 虚函数（禁止 `AddUnion` / `AddIntersect`）。
- 改内核 Pipeline / 分类 / 网格。

---

## 2. 命令

| 项 | Union | Subtract | Intersect |
|----|-------|----------|-----------|
| id | `boolean.union` | `boolean.subtract` | `boolean.intersect` |
| 结果名 | Fuse | Cut | Common |
| 菜单 | Modeling → Boolean Union / Subtract / Intersect | 同左 | 同左 |
| Ribbon | 布尔面板三按钮 | 同左 | 同左 |
| 视口右键 | Boolean Union | Boolean Subtract | Boolean Intersect |
| 类型 | `Interactive` → `BooleanTwoBodyTool` | 同左 | 同左 |

操作数顺序：**先选 / 先点 = 目标（A）**，**后选 / 后点 = 工具体（B）**。  
差集是 `A − B`；并、交几何上可交换，但特征树与历史标签仍按 A/B 记录。

---

## 3. 交互逻辑

`AllowsViewportSelection()` 为 false：工具自己拾取，不走普通框选。第二步 `PickClosestRenderable(..., skip=target)`，便于目标包在工具里面时点到另一体。

### 3.1 状态机

```text
[Start]
   │  已选 ≥2 个不同体 ──► 立即 TryCommit ──► 结束 / 失败则结束并报错
   │  已选 1 个体     ──► [SelectTool]
   │  空选            ──► [SelectTarget]
   ▼
[SelectTarget]  左键命中体 ──► [SelectTool]
[SelectTool]    左键另一体 ──► 两操作数就绪（尚未提交）
                Enter / 右键确定 ──► TryCommit
                ESC / 右键取消   ──► 结束
```

### 3.2 步骤与提示（英文源文）

`%1` = `Union` / `Subtract` / `Intersect`。

| 步骤 | `m_step` | 状态栏 |
|------|----------|--------|
| 选目标 | `SelectTarget` | `Boolean %1: select the first object (target) (ESC cancel)` |
| 选工具 | `SelectTool`（尚未两点） | `Boolean %1: select the second object (hold Ctrl to multi-select), then press Enter (ESC cancel)` |
| 待确认 | `SelectTool`（两点已齐） | `Boolean %1: press Enter to confirm, or right-click Confirm (ESC cancel)` |

未命中：`No object under cursor — pick a body`。  
点到目标自身：`Select a different object as the tool (hold Ctrl to multi-select)`。  
取消：`Cancelled boolean %1`。

### 3.3 提交与撤销

```text
select_entities({target, tool})
ResolveBooleanOperands  // 选择顺序第 1、2 个 FeatureId
ExecuteBoolean(op, target, tool, historyLabel)
  → Scene.AddBoolean → SyncPartBodies → DocumentHistory undo/redo Feature
```

失败不结束工具（差集：可再点工具体）；预选两体启动时失败则结束并报错（与原差集一致）。

---

## 4. 实现落点

| 文件 | 职责 |
|------|------|
| `apps/viewer/commands/tools/BooleanTwoBodyTool.*` | 三操作共用 ITool（`enum class Step`） |
| `apps/viewer/commands/tools/BooleanExecute.*` | 解析操作数 + `AddBoolean` + 历史 |
| `apps/viewer/commands/BuiltinCommands.cpp` | `BooleanOpCommand<Op>` 一律 Interactive |
| `apps/viewer/ui/ActionCatalog.cpp` | 三条命令同一 tooltip |
| `apps/viewer/ui/ContextMenu.cpp` | 视口右键并 / 差 / 交 |
| `apps/viewer/i18n/xcad_zh_CN.ts` | `BooleanTwoBodyTool` / `MainWindow` / `ActionCatalog` |

---

## 5. 验收

- 空选后点并 / 交 / 差：进入选目标，不报「当前不可执行」。
- 只预选一体再点命令：提示选第二体。
- 按顺序预选两体再点命令：立即出结果，操作体抑制。
- 两体重叠：第二步能点到被挡住的另一体。
- ESC：模型不变。
- Undo / Redo：撤/重做该 `BooleanFeature`。
