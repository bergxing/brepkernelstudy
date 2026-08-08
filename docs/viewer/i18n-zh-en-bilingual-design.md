# XCAD Viewer 中英双语设计方案

**状态**：待评审  
**日期**：2026-08-08  
**范围**：`apps/viewer` 用户可见 UI（不含 B-Rep 内核日志）  
**目标**：支持简体中文 / English，可扩展更多语言

---

## 1. 背景与现状

- 菜单、对话框、工具提示、状态栏等几乎全部使用硬编码中文：`QStringLiteral("保存")`。
- 未接入 `QTranslator` / `.ts` / `.qm`。
- 命令内部 ID（如 `edit.copy`）已是英文稳定标识，适合保留。

---

## 2. 目标与非目标

### 2.1 目标（v1）

| 项目 | 要求 |
|------|------|
| 语言 | `zh_CN`（简体中文）、`en`（English） |
| 覆盖 | 菜单、工具栏、对话框、状态栏短提示、交互工具 `prompt()`、右键菜单、Home/启动相关文案 |
| 切换 | 设置项选择语言；**重启后生效**（v1） |
| 默认 | 跟随系统 UI 语言；无法识别时默认 `zh_CN`（当前用户主体）或 `en`（可配置） |
| 构建 | CMake 生成/安装 `.qm`，运行时可从 exe 旁 `translations/` 加载 |

### 2.2 非目标（v1 不做）

- 运行时无重启热切换全 UI（可作 v2）
- 内核 / spdlog 诊断日志多语言
- 命令行参数本地化、文件格式字符串本地化
- 专业 TMS（Crowdin 等）；v1 用 Qt Linguist 即可
- 右到左（RTL）布局

---

## 3. 技术选型（与主流一致）

采用 **Qt Linguist 官方链路**（AutoCAD/多数 Qt 桌面应用同类思路）：

```
源码 tr("Save")  --lupdate-->  en.ts / zh_CN.ts  --Linguist-->  --lrelease-->  *.qm
                                                                  ↓
启动时 QTranslator::load("xcad_zh_CN.qm") + qApp->installTranslator()
```

| 方案 | 说明 | 结论 |
|------|------|------|
| **A. Qt `tr` + `.ts/.qm`** | 标准、工具成熟、和 Widgets 契合 | **采用** |
| B. 自研 JSON 词条表 | 灵活但要自建工具与上下文 | 不采用 |
| C. gettext | 跨框架常见，Qt 集成成本更高 | 不采用 |

---

## 4. 源语言策略（重要）

**推荐：源码字符串使用英文，中文放在 `zh_CN.ts`。**

| | 英文作源语言 | 中文作源语言 |
|--|-------------|-------------|
| 长期扩展 | 加日/韩/德自然 | 每次先译成英文再扩散，别扭 |
| Linguist 习惯 | 源=英文是常态 | 可用但少见 |
| 当前迁移成本 | 需把现有中文改成 `tr("...")` 英文 | 改动小，先提 `en.ts` |
| 代码可读性（国际协作） | 更好 | 对中文团队暂时更熟 |

**v1 决策：采用英文源语言（方案 A 迁移）。**  
若希望更快出包，可临时「中文源 + en.ts」，但本方案按长期正确路径写实施步骤。

示例：

```cpp
// Before
menu->addAction(QStringLiteral("保存(&S)"));

// After
menu->addAction(tr("Save(&S)"));  // zh_CN.ts: "保存(&S)"
```

快捷键助记符 `&S`：中英都可保留；中文译成 `保存(&S)` 即可。

---

## 5. 架构设计

### 5.1 模块与翻译上下文

| Context（`tr` 所在类/disambiguation） | 内容 |
|--------------------------------------|------|
| `MainWindow` | 菜单、工具栏、关闭保存框、光标提示 idle 文案 |
| `HomeWindow` / `SplashScreen` | 启动与首页 |
| `brep::viewer::commands` | 命令显示名、工具 `prompt()`、结果消息 |
| `PropertyPanel` 等 | 属性面板标签 |

非 QObject 工具类使用：

```cpp
QCoreApplication::translate("CreateBoxTool", "Create box: pick first base corner");
```

### 5.2 文件布局

```
apps/viewer/
  i18n/
    xcad_en.ts          # 可选：英文也可作校对用（源已是英文时可极薄）
    xcad_zh_CN.ts       # 简体中文
  ...
```

运行时部署：

```
brep_viewer.exe
translations/
  xcad_en.qm
  xcad_zh_CN.qm
```

也可加载 Qt 自带的 `qtbase_*.qm`（标准对话框按钮 OK/Cancel 等）。

### 5.3 语言设置存储

- 使用 `QSettings`（组织名 `XCAD`，应用名 `brep_viewer` 或 `XCAD`）
- Key：`ui/language`，值：`system` | `zh_CN` | `en`
- 解析顺序：
  1. 用户显式选择
  2. `system` → `QLocale::system().name()`（`zh_CN` / `en_US` → 映射到 `zh_CN` / `en`）
  3. fallback：`zh_CN`

### 5.4 启动加载流程

```
main()
  → 读 QSettings
  → 决定 language_tag
  → installTranslator(qtbase_xx.qm)   // 可选
  → installTranslator(xcad_xx.qm)
  → 再创建 HomeWindow / MainWindow
```

**注意**：Translator 必须在创建带 `tr()` 的窗口**之前**安装。

### 5.5 设置 UI（v1）

- 菜单：`编辑` 或 `工具` → `语言(&L)…` / `Language…`
- 或 Home / 首选项简单对话框：下拉「跟随系统 / 简体中文 / English」
- 变更后提示：**「语言将在重启后生效」**，写入 settings，不强制立刻 `retranslate`

---

## 6. 字符串分类规范

| 类型 | 是否翻译 | 写法 |
|------|----------|------|
| 菜单、按钮、对话框 | 是 | `tr("Save")` |
| 工具逐步提示 | 是 | `tr("Copy: select objects, then press Space")` |
| 状态栏用户消息 | 是 | `tr("Selected: %1").arg(label)` |
| 命令 ID | 否 | `"edit.copy"` |
| 日志 BREP_INFO/WARN | 否 | 英文 |
| 文件过滤器 | 是 | `tr("XCAD Document (*.xl)")` |
| 物体内部 name（box） | 否（或仅显示层） | 保持数据层英文 |

带参数时用 `%1`，勿用字符串拼接语序（中英语序不同）：

```cpp
tr("%1 object(s) selected").arg(count);  // 中文译："已选中 %1 个对象"
```

---

## 7. 构建集成（CMake）

建议在 `apps/viewer/CMakeLists.txt`：

1. `find_package(Qt6 COMPONENTS LinguistTools)`（若可用）
2. `qt_add_translations(brep_viewer TS_FILES i18n/xcad_zh_CN.ts i18n/xcad_en.ts)`
   - 或手写 `lupdate` / `lrelease` custom target
3. POST_BUILD 将 `.qm` 复制到 `$<TARGET_FILE_DIR:brep_viewer>/translations/`

开发者日常：

```text
改 UI 字符串 → 构建或手动 lupdate → Linguist 填中文 → lrelease → 运行验证
```

CI：可校验 `zh_CN.ts` 无未完成条目（可选，v1 不强制）。

---

## 8. 迁移实施计划（分阶段）

### Phase 0 — 骨架（0.5–1 天）

- [ ] 增加 `i18n/` 与空 `xcad_zh_CN.ts`
- [ ] `main.cpp`：`load_translators(lang)`
- [ ] `QSettings` 读写语言
- [ ] CMake 复制 `.qm`
- [ ] 设置入口（菜单一项即可）
- [ ] 用 **少量** 字符串验证中英切换（重启）

### Phase 1 — 主窗口与对话框（1–2 天）

- [ ] `MainWindow` 菜单/工具栏/关闭保存框/右键菜单
- [ ] `HomeWindow` / `SplashScreen`
- [ ] `QMessageBox` 标题与正文

### Phase 2 — 命令与工具（1–2 天）

- [ ] `builtin_commands` 显示名（若有）
- [ ] `CreateBoxTool` / `CopyTool` 的 `prompt()` 与用户可见失败信息
- [ ] 命令面板条目（若显示本地化标题）

### Phase 3 — 收尾（0.5–1 天）

- [ ] 属性面板、状态栏剩余文案
- [ ] 光标跟随提示（工具 prompt 已译则自动覆盖）
- [ ] Linguist 通校中文
- [ ] 默认语言与「跟随系统」实测（中文 Windows / 英文 Windows）

**预估总工作量**：约 3–5 人日（含翻译校对）。

---

## 9. 代码改造约定（给实现用）

1. **禁止**新增用户可见的裸 `QStringLiteral("中文")`；一律 `tr("English")`。
2. QWidget 子类用 `tr()`；自由函数/工具类用 `QCoreApplication::translate("Context", "...")`。
3. 动态组句只用 `%n` / `%1`，并在 `.ts` 里写自然中文语序。
4. 不翻译的标识用 `QLatin1String` / `std::string`，避免误抽进 `.ts`。
5. 改字符串后跑一次 `lupdate`，避免 `.ts` 过期。

辅助宏（可选）：

```cpp
// apps/viewer/i18n.hpp
#define XCAD_TR(ctx, src) QCoreApplication::translate(ctx, src)
```

---

## 10. 测试计划

| 用例 | 期望 |
|------|------|
| 设置 English → 重启 | 菜单为 Save / Edit / Copy… |
| 设置 简体中文 → 重启 | 菜单为 保存 / 编辑 / 复制… |
| 跟随系统（中文 OS） | 启动为中文 |
| 跟随系统（英文 OS） | 启动为英文 |
| 关闭未保存文档 | 对话框三按钮随语言变化 |
| 复制/创建立方体工具 | 逐步提示随语言变化 |
| 缺 `.qm` 文件 | 回退到源语言（英文），不崩溃 |

---

## 11. 风险与缓解

| 风险 | 缓解 |
|------|------|
| 漏改硬编码中文 | Phase 结束前全文搜 `QStringLiteral("[\u4e00-\u9fff]` |
| 助记符 `&` 冲突 | 中英分别校对菜单字母 |
| Translator 装太晚 | 只在 `main()`、创建窗口前加载 |
| 热切换期待 | 文档与设置文案写明「重启生效」 |

---

## 12. v2 展望（本方案不实施）

- 不重启 `retranslateUi` / 重建菜单
- 更多语言包（`ja_JP` 等）
- 命令本地化别名（输入「复制」触发 `edit.copy`）
- 文档/示例工程多语言

---

## 13. 决策确认清单

请确认后开始 Phase 0 实现：

1. **源语言用英文**，`zh_CN.ts` 提供中文 — 是否同意？  
2. **切换语言重启生效** — 是否同意？  
3. **默认**：跟随系统，未知则 `zh_CN` — 是否同意？  
4. **设置入口**：放在「工具」菜单还是独立「首选项」对话框？  

确认后按 Phase 0 → 1 → 2 → 3 落地，不必一次性改完所有字符串。
