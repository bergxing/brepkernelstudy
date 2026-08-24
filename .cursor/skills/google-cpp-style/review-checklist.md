# C++ 风格 — 审查清单

## 命名

- [ ] 类 / struct 类型 / 函数：**PascalCase**
- [ ] 变量 / 参数：**camelCase**（无 `m_`）
- [ ] **类成员变量**：**`m_` + camelCase**（禁止 `m_PascalCase`、`member_`）
- [ ] struct / DTO 成员：**PascalCase（大驼峰）**（无 `m_`；**禁止** `ok`/`failed`/`message`/`status` 等 snake_case）
- [ ] 接口 `I` + PascalCase；常量 kPascalCase

## 格式

- [ ] **4 空格**缩进（**禁止 2 空格 / Tab**；类成员 4 空格、函数体 8 空格起）
- [ ] **Allman** 括号（`{` 下一行）
- [ ] 行宽 ≤80（合理例外）
- [ ] `nullptr`；无 C 风格 cast

## 头文件

- [ ] 文件名 **PascalCase**；扩展名 `.h` / `.cpp`
- [ ] 自包含；IWYU；`#pragma once`
- [ ] include 顺序正确
- [ ] Viewer 仅用 `api/*`（生产代码）

## 类与函数

- [ ] 单参 `explicit`；虚函数 `override`/`final`
- [ ] 数据成员 private
- [ ] 公共 API 有必要注释

## Core Guidelines 联合项

- [ ] 所有权清晰（R.*）
- [ ] `[[nodiscard]]` 于 Result/bool
- [ ] 错误可诊断
- [ ] 无裸 new/delete 散落业务代码
