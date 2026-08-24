# C++ 风格 — 主题索引

项目约定见 [project-overrides.md](project-overrides.md)。Google 原文：https://google.github.io/styleguide/cppguide.html

## Naming

| 元素 | 风格 |
|------|------|
| 类 / struct 类型 / 函数 | PascalCase |
| 变量 / 参数 | camelCase |
| **类成员变量** | **`m_` + camelCase** |
| struct / DTO 成员 | PascalCase |
| 接口 | `I` + PascalCase |
| 常量 | kPascalCase |
| 宏 | UPPER_CASE |
| 文件 | **PascalCase**（**`.h`** / `.cpp`） |

## Formatting

- **4 空格**缩进
- **Allman** 括号：`{` 下一行
- 80 列行宽
- 指针/引用贴类型：`Type* p`

## Headers

- 自包含；IWYU
- `#pragma once`
- include 顺序：自身 → 项目 → 标准库/第三方 → Qt

## Classes

- 组合优于继承；接口用抽象基类 + 虚析构
- 单参 `explicit`；重写 `override`/`final`
- 成员 private；**类成员 `m_` + camelCase**；struct / DTO 成员 PascalCase

## Functions

- PascalCase 名称
- `[[nodiscard]]` 于 Result/bool
- 工厂：`Make*` / `Create*`

## 与 Core Guidelines

| 风格 | Guideline |
|------|-----------|
| explicit | C.46 |
| override | C.128 |
| 智能指针 | R.20–R.21 |
| const 成员函数 | Con.3 |
