# Agent 指南 — brepkernelstudy

## 项目结构

- `kernel/` — B-Rep 内核（`brep_core`, `brep_feat`, `brep_io`, `brep_asm`）
- `apps/viewer/` — Qt6 + Vulkan Viewer
- `tests/` — GoogleTest 内核测试
- `docs/architecture/` — ADR 与工程规范

## C++ 规范

两套规范并行：**Google Style**（格式/命名/头文件）+ **Core Guidelines**（语义/所有权/接口）。

### Google C++ Style

- **Cursor rules**：`.cursor/rules/google-cpp-*.mdc`
- **Agent skill**：`.cursor/skills/google-cpp-style/SKILL.md`
  - 编写/审查 C++ 格式、命名、头文件、类布局时 **先读该 skill**
  - 项目约定见 `google-cpp-style/project-overrides.md`（**PascalCase 文件名 + `.h`/`.cpp`**、PascalCase 类/函数、camelCase 变量/参数、**类成员 `m_` + camelCase**、struct/DTO PascalCase、**4 空格缩进（禁止 2 空格/Tab）+ Allman**）
  - 迁移脚本：`scripts/migrate_style.py`（`.hpp`→`.h`、snake_case→PascalCase）、`scripts/fix_includes.py`（统一 `#include`）；**勿用**已废弃的 `fix_includes_to_hpp.py`、`format_allman_braces.py`（会破坏 `const`/`override`/`/*name*/`）。若已误跑，执行 `python scripts/repair_allman_corruption.py` 修复

### C++ Core Guidelines

- **Cursor rules**：`.cursor/rules/cpp-*.mdc`
- **Agent skill**：`.cursor/skills/cpp-core-guidelines/SKILL.md`
  - RAII、接口设计、错误处理时 **先读该 skill**
  - 项目特例见 `cpp-core-guidelines/project-overrides.md`

## 关键边界

- Viewer 生产代码仅 `#include "api/..."`（见 ADR 0002）
- 布尔架构：ADR 0006（Pipeline）；IoC：ADR 0007（Hypodermic，Viewer only）
- 构建：`cmake-build-mingw-debug`，MinGW 在 PATH

## 测试

依赖 **GoogleTest**（`third_party/googletest`，git submodule，tag v1.15.2）。Configure 时**不再**从 GitHub 在线下载。

```powershell
git submodule update --init third_party/googletest
```

若 GitHub 不可达，submodule 已配置 Gitee 镜像。也可手动将 v1.15.2 解压到 `third_party/googletest/`。

临时跳过测试：`cmake -DBREP_BUILD_TESTS=OFF ...`

```powershell
ctest -C Debug --output-on-failure
ctest -R include_boundaries
```
