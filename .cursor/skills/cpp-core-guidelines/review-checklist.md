# Core Guidelines 代码审查清单

审查 `kernel/` / `viewer/` / `tests/` 变更时使用。

## 正确性与安全

- [ ] 无未定义行为（空指针解引用、越界、use-after-free）
- [ ] 所有权清晰：谁分配、谁释放、是否非拥有指针
- [ ] 无裸 `new`/`delete`（工厂内 `make_unique` 除外）
- [ ] 多态基类有 `virtual ~T()` 或 protected 析构
- [ ] `[[nodiscard]]` 用于 Result / error / 查询 API

## 接口与 API

- [ ] 新抽象是否 `I*` 接口 + 实现分离（IoC/Pipeline 友好）
- [ ] 单参构造 `explicit`
- [ ] 参数传递合理（F.16：小对象值传递，大对象 const&）
- [ ] 不暴露 `kernel/internal` 到 Viewer

## 错误处理

- [ ] 失败路径有 `diagnostics` / 日志 / 用户消息
- [ ] 无空 catch；无静默忽略 `BooleanResult`
- [ ] 测试断言失败原因，不只断言 `!ok()`

## 风格与边界

- [ ] 命名符合 project-overrides（PascalCase 类/函数、camelCase 变量、`m_` 成员）
- [ ] Viewer 仅 `api/*` includes
- [ ] `#pragma once`；include 顺序合理
- [ ] 变更范围最小，无无关重构

## 反馈分级

| 级别 | 含义 |
|------|------|
| **Critical** | UB、泄漏、API 边界破坏 — 必须修 |
| **Major** | 违反 R/I/E 核心条文 — 应修 |
| **Minor** | 可读性/const/nodiscard — 建议 |
| **Note** | 与 Guideline 一致但非必须 |

## 示例评论

```
Major (R.11): 此处 raw new 应改为 make_unique，所有权交给 Model。
Major (I.11): 返回 Body* 的注释应标明非拥有，调用方不得 delete。
Minor (Con.2): 只读查询方法应标记 const。
Critical (SF.*): viewer/commands 直接 include brep/Part.h，应改用 api/Modeling.h。
```
