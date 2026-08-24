# brepkernelstudy — Core Guidelines 项目覆盖

Guideline 与仓库决策冲突时，以本文为准。格式/命名/头文件见 `.cursor/skills/google-cpp-style/project-overrides.md`。

## 语言与构建

- **C++20**（`cmake/BrepKernelIncludes` / target `cxx_std_20`）
- 编译器：MinGW GCC、MSVC（见 CI/本地 preset）
- 库：Eigen、EnTT、Qt6（Viewer）、spdlog

## 命名

与 `google-cpp-style/project-overrides.md` 一致：

| 元素 | 约定 |
|------|------|
| 命名空间 | `brep::`, `brep::boolean::`, `brep::viewer::` |
| 类/结构体类型、函数 | PascalCase |
| 接口 | `I` + PascalCase |
| 变量/参数 | camelCase |
| 类成员 | `m_` + camelCase |
| struct 数据成员 | PascalCase |
| 枚举类 | PascalCase 成员 |
| 工厂 | `Make*` / `Create*` |

## 模块边界

```
brep_core → brep_feat → brep_asm / brep_io → brep (INTERFACE)
apps/viewer: viewer_adapter → viewer_runtime → viewer_ui → brep_viewer
```

- Viewer **仅** `api/*` 公开头（`bootstrap/`、tests 例外）
- Kernel **不** 链接 Hypodermic（ADR 0007）
- 内部头：`kernel/internal/`（PRIVATE）

## 所有权模式（已落地）

| 对象 | 所有权 |
|------|--------|
| `Model` 内拓扑 | `unique_ptr` 容器 |
| `Document` → `Part` | `vector<unique_ptr<Part>>` |
| `IBooleanEvaluator` | `shared_ptr`，**Part 构造注入**（IoC 方案） |
| `Body*` 返回 | 非拥有，归 `Model` |
| ECS `BodyRef` | 仅 `Guid` |

## 错误处理（项目惯用法）

- Kernel 布尔：`BooleanResult` + `diagnostics`，`ok()` == body 非空
- Feature 再生：`bool` + `Part::last_regen_detail()`
- Viewer：`CommandResult` + 中文用户消息
- 日志：`BREP_INFO/WARN/ERROR`（spdlog）

## 不建议在本项目使用

- 异常作为常规控制流（kernel 热路径）
- 全局可变单例（除明确 Composition Root / 日志）
- Viewer 直接 include `brep/bool/*.hpp`
- `std::auto_ptr`、C 风格 cast 主导

## 相关 ADR

- ADR 0002：`api/*` 分级
- ADR 0003：SHARED 库
- ADR 0006：Boolean Pipeline
- ADR 0007：Hypodermic（Viewer only）
