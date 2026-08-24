---
name: cpp-core-guidelines
description: >-
  Apply C++ Core Guidelines (isocpp.github.io) when writing, reviewing, or
  refactoring C++ in brepkernelstudy. Covers ownership (R.*), interfaces (I.*),
  functions (F.*), error handling (E.*), constants (Con.*), and project-specific
  overrides for kernel/viewer/api boundaries. Use when the user mentions Core
  Guidelines, cppcoreguidelines, C++ best practices, RAII, or asks for C++
  code review in this repo.
---

# C++ Core Guidelines（brepkernelstudy）

## When to use

- 编写/审查 `kernel/`、`apps/viewer/`、`tests/` 下的 C++
- 用户提到 Core Guidelines、RAII、接口设计、错误处理
- PR review、重构 `Part`/布尔 Pipeline/Viewer 命令

## Quick workflow

1. **读项目覆盖**：[project-overrides.md](project-overrides.md)
2. **按场景查表**：[reference.md](reference.md) 中的 Guideline 索引
3. **审查**时用 [review-checklist.md](review-checklist.md)
4. **Cursor rules** 已拆分：`.cursor/rules/cpp-*.mdc`

## 权威来源

- 在线：https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- 本项目 **不替代** Guideline 全文；冲突时：**项目 ADR/边界 > 仓库惯例 > Guideline**

## 编写新代码（最小清单）

```
- [ ] 所有权清晰（R.*）：谁 create / 谁 destroy
- [ ] 接口 I.*：抽象用 I* + virtual ~T
- [ ] [[nodiscard]] 于 Result/bool/指针返回
- [ ] const 正确（Con.*）
- [ ] 无裸 new/delete（R.11）
- [ ] 错误可诊断（E.* / BooleanResult / CommandResult）
- [ ] Viewer 只用 api/*（边界）
- [ ] 测试覆盖失败路径 diagnostics
```

## 与 Google C++ Style 的关系

- 项目 skill：`.cursor/skills/google-cpp-style/SKILL.md` — **格式、命名、头文件、类布局**
- **本 skill 偏语义与安全**（Core Guidelines）
- 命名/格式以 **本仓库**为准（见 `google-cpp-style/project-overrides.md`：PascalCase 类/函数、camelCase 变量、`m_` 成员、4 空格 Allman）

## 附加资源

- [reference.md](reference.md) — Guideline 分类索引
- [project-overrides.md](project-overrides.md) — brep 特有约定
- [review-checklist.md](review-checklist.md) — 审查清单
