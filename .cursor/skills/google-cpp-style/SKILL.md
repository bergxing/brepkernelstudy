---
name: google-cpp-style
description: >-
  Apply brepkernelstudy C++ style (based on Google Style Guide) when writing,
  reviewing, or refactoring C++. Covers PascalCase types/functions, camelCase
  variables, m_ members, 4-space Allman braces, headers, and comments. Use when
  the user mentions Google Style, naming, formatting, or C++ code review in this
  repo.
---

# C++ 风格（brepkernelstudy）

## When to use

- 编写/审查 `kernel/`、`apps/viewer/`、`tests/` 下的 C++
- 命名、格式、头文件、类布局
- 与 Core Guidelines 并行的风格审查

## Quick workflow

1. **读项目约定**：[project-overrides.md](project-overrides.md)
2. **主题索引**：[reference.md](reference.md)
3. **审查**：[review-checklist.md](review-checklist.md)
4. **Cursor rules**：`.cursor/rules/google-cpp-*.mdc`
5. **自动格式**：根目录 `.clang-format`（4 空格 + Allman）

## 命名速查

| 元素 | 风格 |
|------|------|
| 类 / 结构体 / 函数 | PascalCase |
| 变量 / 参数 | **camelCase**（无 `m_`） |
| **类成员变量** | **`m_` + camelCase** |
| **struct / DTO 成员** | **PascalCase（大驼峰，强制）** — 禁止 `ok`/`failed`/`message` 等 snake_case |

## 格式速查

- **4 空格缩进（强制）** — 每一级嵌套 +4 空格；**禁止 Tab**、**禁止 2 空格**
- `{` 在声明下一行（Allman）
- 80 列行宽
- 编写/修改代码后可用 `clang-format -i <file>`（遵循根目录 `.clang-format`）

## 编写新代码（最小清单）

```
- [ ] 头文件自包含；IWYU；#pragma once
- [ ] 类/函数 PascalCase；变量/参数 camelCase
- [ ] **类成员 `m_` + camelCase**（勿与 struct 字段混淆）
- [ ] struct / DTO 成员 PascalCase
- [ ] explicit；override/final
- [ ] 4 空格、Allman 括号
- [ ] 与 Core Guidelines 并行（所有权/接口/错误）
```

## 与 Core Guidelines

- **本 skill**：命名、格式、头文件、类布局
- **Core Guidelines skill**：语义、所有权、错误处理

## 附加资源

- [project-overrides.md](project-overrides.md)
- [reference.md](reference.md)
- [review-checklist.md](review-checklist.md)
- Google 原文：https://google.github.io/styleguide/cppguide.html
