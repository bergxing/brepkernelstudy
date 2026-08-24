# C++ Core Guidelines — 分类索引

完整条文：https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines

## 本仓库高频章节

### P — Philosophy

| ID | 要点 |
|----|------|
| P.4 | Ideally correct before optimization |
| P.5 | Compile-time validation |
| P.10 | Prefer immutable data |

### I — Interfaces

| ID | 要点 |
|----|------|
| I.1 | 接口 = 抽象类 |
| I.4 | 可静态检查 |
| I.11 | 非拥有指针不 delete |
| I.12 | 非空用引用 |
| I.23 |  polymorphic 类需 virtual 析构 |
| I.27 | 依赖接口 |

### F — Functions

| ID | 要点 |
|----|------|
| F.1 | 单一职责 |
| F.6 | `noexcept` 若不会 throw |
| F.7 | `unique_ptr` 返回堆对象 |
| F.8 | Prefer pure functions |
| F.16 | 参数传递：cheap in |
| F.20 | out 参数：非 const 引用 |
| F.43 | 禁止 UB 参数 |

### R — Resource management

| ID | 要点 |
|----|------|
| R.1 | RAII |
| R.3 | 非 RAII → `unique_ptr`/`shared_ptr` |
| R.5 | 局部 scoped 对象 |
| R.10 | 避免 malloc/free |
| R.11 | 避免裸 new/delete |

### C — Classes

| ID | 要点 |
|----|------|
| C.2 | 类表示不变量 |
| C.21 | 定义全部特殊成员或 none（Rule of 0/5） |
| C.35 | 基类析构 virtual |
| C.46 | 单参 explicit |
| C.67 | 多态类 suppress 拷贝或 protected |

### ES — Expressions

| ID | 要点 |
|----|------|
| ES.20 | 初始化 |
| ES.23 | 理解 move 语义 |
| ES.28 | 用 lambda 替代 tiny 函数对象（适度） |

### E — Error handling

| ID | 要点 |
|----|------|
| E.2 | 不用 throw 做预期分支 |
| E.12 | 不空 catch |
| E.14 | 专用错误类型/通道 |

### Con — Constants

| ID | 要点 |
|----|------|
| Con.1 | 默认 const |
| Con.2 | 读操作用 const |
| Con.5 | `constexpr` 编译期常量 |

### T — Templates

| ID | 要点 |
|----|------|
| T.1 | 模板提升泛型，非运行时多态替代 |
| T.10 | 概念/约束（C++20 `concept` 可用处） |
| T.120 | 只在需要多态行为时用继承 |

### SF — Source files

| ID | 要点 |
|----|------|
| SF.1 | Include guard / `#pragma once` |
| SF.2 | Include 顺序 |
| SF.7 | 最小 include |
| SF.11 | 头文件自洽 |

### CP — Concurrency

| ID | 要点 |
|----|------|
| CP.1 | 线程共享需明确 |
| CP.2 | const 线程安全 |
| CP.3 | 最小共享 |

## 工具（可选）

- **Guideline Support Library (GSL)**：`span`, `not_null` — 本项目未全面引入，参考思想即可
- **静态分析**：clang-tidy `cppcoreguidelines-*` checks（CI 可逐步启用）
