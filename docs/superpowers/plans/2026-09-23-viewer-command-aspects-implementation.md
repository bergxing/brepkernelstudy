# 工程边界切面 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在层边界上用同一条拦截器链记录日志、耗时和失败，并为每个窗口的命令另加状态栏切面。

**Architecture:** `brep::AspectChain` 放在 `brep_base`。内核边界和 `SceneAdapter` 的修改操作调用进程链。`CommandManager` 持有窗口链，其上多一个 Status。Hypodermic 只在启动时安装这两条链，执行路径不再 `resolve`。

**Tech Stack:** C++20、现有 `brep_base` / `viewer_runtime` / `viewer_bootstrap`、GoogleTest、MinGW preset `cmake-build-mingw-debug`。不新增依赖。

## Global Constraints

- 切面运行时不 include Qt 或 Hypodermic。`CommandPayload` 只存在于 `apps/viewer/commands/`。
- 不引入 AOP 库或编译期织入。
- 切面不改返回值、不改模型、不实现撤销。
- 连接点只包括规格第 5 节的目录。鼠标、键盘、右键、Vulkan 帧、单个面、单个三角形不包。
- `Before` 正序，`After` / `OnError` 逆序。`Before` 未返回的切面不收到 `OnError`。异常继续抛出。`Failed` 走 `After`。
- 新类型 PascalCase，类成员 `m_` + camelCase，4 空格，Allman，`#pragma once`。
- 生产日志走 `BREP_INFO` / `BREP_ERROR`。

**规格：** [2026-09-23-viewer-command-aspects-design.md](../specs/2026-09-23-viewer-command-aspects-design.md)  
**决策：** [ADR 0012](../../architecture/adr/0012-viewer-command-aspects.md)

本计划按层拆开。每一阶段结束时现有 ctest 仍通过，新测试单独可跑。不要把后一阶段的连接点提前塞进前一阶段。

---

### Task 1: brep_base 里的 AspectChain

**Files:**
- Create: `kernel/include/brep/Aspect.h`
- Create: `kernel/src/base/Aspect.cpp`
- Modify: `kernel/include/api/Base.h`（在 `Log.h` 旁边 include `brep/Aspect.h`）
- Modify: `cmake/BrepBase.cmake`（把 `Aspect.cpp` 加进 `brep_base`）
- Create: `tests/kernel/TestAspect.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces: `brep::IAspect`、`brep::AspectEvent`、`brep::AspectChain::Add`、`AspectChain::Invoke`、`AspectChain::Names`

- [ ] **Step 1: 写失败测试**

`tests/kernel/TestAspect.cpp` 用一个把 `Name:Before:Site` 记进 `std::vector<std::string>*` 的切面。断言：

1. 空链执行 `body`，返回值原样传出。
2. 两个切面 A、B：`A:Before:demo`、`B:Before:demo`、`B:After:demo`、`A:After:demo`。`body` 把 `event.Failed` 设为真后，两条 `After` 记录都带 `:Failed`。
3. `body` 抛 `std::runtime_error("boom")`：两条 `Before`，然后 `B:OnError:demo:boom`、`A:OnError:demo:boom`，没有 `After`，异常仍抛出。

在 `tests/CMakeLists.txt` 增加 `brep_test_aspect`，链接 `brep` 与 `GTest::gtest_main`，`gtest_discover_tests` 的工作目录与现有 `brep_test_math` 相同。

- [ ] **Step 2: 编译测试目标，确认失败**

```powershell
cmake --build E:\brepkernelstudy\cmake-build-mingw-debug --target brep_test_aspect
```

Expected: 找不到 `brep::AspectChain`。

- [ ] **Step 3: 实现链**

`Aspect.h` 声明规格第 3 节的三个类型。`Invoke` 放在头文件里，因为是模板：

```cpp
template <class F>
decltype(auto) AspectChain::Invoke(AspectEvent& event, F&& body)
{
    std::size_t entered = 0;
    try
    {
        for (const auto& aspect : m_aspects)
        {
            aspect->Before(event);
            ++entered;
        }
        decltype(auto) result = std::forward<F>(body)();
        for (std::size_t i = m_aspects.size(); i > 0; --i)
        {
            m_aspects[i - 1]->After(event);
        }
        return result;
    }
    catch (const std::exception& error)
    {
        for (std::size_t i = entered; i > 0; --i)
        {
            m_aspects[i - 1]->OnError(event, error);
        }
        throw;
    }
}
```

`void` 的 `body` 不能用上面的 `return result`。再提供一个 `void Invoke(AspectEvent&, F&&)` 的重载，用 `if constexpr (std::is_void_v<std::invoke_result_t<F&>>)` 分成两条路径，异常路径相同。`m_aspects` 是 `std::vector<std::unique_ptr<IAspect>>`。

- [ ] **Step 4: 跑测试**

```powershell
cmake --build E:\brepkernelstudy\cmake-build-mingw-debug --target brep_test_aspect
ctest --test-dir E:\brepkernelstudy\cmake-build-mingw-debug -R "brep_test_aspect" --output-on-failure
```

Expected: PASS。

- [ ] **Step 5: Commit**

```powershell
git add kernel/include/brep/Aspect.h kernel/src/base/Aspect.cpp kernel/include/api/Base.h cmake/BrepBase.cmake tests/kernel/TestAspect.cpp tests/CMakeLists.txt
git commit -m "feat: add a boundary aspect chain in brep_base"
```

---

### Task 2: 进程链与三个内核切面

**Files:**
- Modify: `kernel/include/brep/Aspect.h`
- Modify: `kernel/src/base/Aspect.cpp`
- Modify: `tests/kernel/TestAspect.cpp`

**Interfaces:**
- Consumes: `AspectChain`
- Produces: `ProcessAspectChain()`、`ScopedProcessAspectChain`、`InstallProcessAspects()`、`SetAspectTraceSinkForTest`、`LoggingAspect`、`TimingAspect`、`ErrorAspect`（都在命名空间 `brep`）

- [ ] **Step 1: 写失败测试**

1. `ScopedProcessAspectChain` 放入一个记录切面后，`ProcessAspectChain().Names()` 含该名字；守卫析构后名字列表恢复。
2. `InstallProcessAspects()` 之后名字顺序为 `Timing`、`Logging`、`Error`。再调用一次仍然是这三个，不是六个。
3. 进程链上 `Invoke` 一个 `Site="part.regenerate"`、`Subject="box"` 的事件，trace sink 收到含 `part.regenerate` 与 `elapsed_ms=` 的 info 行。`Failed=true` 时另有一条 error 行，含 `box`。

`SetAspectTraceSinkForTest` 的类型是 `void(*)(bool isError, const char* message)`。测试结束传 `nullptr`。

- [ ] **Step 2: 跑测试，确认失败**

Expected: `InstallProcessAspects` 未声明。

- [ ] **Step 3: 实现**

进程链是函数内的 `static AspectChain`。`ScopedProcessAspectChain` 保存旧链的切面指针所有权：把当前链 `Release()` 到守卫里，析构时 `Restore`。为此 `AspectChain` 增加 `Release()` 与 `Reset(vector<unique_ptr<IAspect>>)`。不要复制切面对象。

`InstallProcessAspects` 先 `Reset({})`，再 `Add` 三个切面。

三个切面的 `After` / `OnError` 调用内部的 `WriteAspectTrace(bool isError, std::string_view)`。该函数先 `BREP_INFO` 或 `BREP_ERROR`，再调用测试 sink。

- Logging 的 `After`：`site '{Site}' subject '{Subject}'`
- Timing：成员里按 `Site` + 指针没有共享状态需求，用 `Before` 记下 `steady_clock`，键为 `Site` 与本次 `Invoke` 的地址做不到。改成切面内一个 `std::vector` 栈：`Before` push 时间与 `Site`，`After` / `OnError` pop 并写 `elapsed_ms=`。嵌套 `Invoke` 因此是安全的。
- Error 的 `After`：仅 `event.Failed` 时写 `site '{Site}' subject '{Subject}' failed: {Detail}`。`OnError` 写 `exception: {what()}`。

- [ ] **Step 4: 跑 `brep_test_aspect`**

Expected: PASS。

- [ ] **Step 5: Commit**

```powershell
git add kernel/include/brep/Aspect.h kernel/src/base/Aspect.cpp tests/kernel/TestAspect.cpp
git commit -m "feat: install process-wide logging timing and error aspects"
```

---

### Task 3: 内核四个边界

**Files:**
- Modify: `kernel/src/Part.cpp`（`Part::Regenerate`）
- Modify: `kernel/src/bool/Pipeline.cpp`（`BooleanPipeline::Evaluate` 中每个 `stage->Run`）
- Modify: `kernel/src/io/XlDocument.cpp`（`LoadXl`、`SaveXl`）
- Modify: `kernel/src/mesh/Mesh.cpp`（`TessellateBody`）
- Modify: 现有布尔、xl、网格测试，只在断言条数被日志打乱时调整；不放宽几何断言

**Interfaces:**
- Consumes: `ProcessAspectChain().Invoke`

- [ ] **Step 1: 写失败测试**

在 `tests/kernel/TestAspect.cpp` 不重复造几何。各加一个小测试，或加进现有最窄的测试文件：

- 新建一个空 `Part` 调用 `Regenerate()`。trace 里有一条 `part.regenerate`。
- 用现有测试里最小的两个盒子做一次 `BooleanPipeline::Evaluate`。trace 中 `boolean.pipeline` 的 `Subject` 顺序包含 `Preprocess`、`Intersect`、`Imprint`、`Classify`、`Select`、`Build`（与 `PipelineStageName` 的返回值一致）。
- `TessellateBody` 对一个盒子：一条 `mesh.tessellate`。
- 把一个只含盒子的文档 `SaveXl` 到临时文件再 `LoadXl`：各一条 `io.xl.save`、`io.xl.load`。

这些测试开头用 `ScopedProcessAspectChain`，链上放一个记录切面，不要依赖进程链里已经安装的 Logging，避免和默认安装打架。记录切面直接实现 `IAspect`。

- [ ] **Step 2: 跑这些测试，确认失败**

Expected: 记录为空，因为边界还没有 `Invoke`。

- [ ] **Step 3: 在边界包一层**

每个入口只包一层，不要包进循环内部的辅助函数。布尔阶段示例：

```cpp
AspectEvent event;
event.Site = "boolean.pipeline";
event.Subject = PipelineStageName(stage->Id());
PipelineStageResult stageResult;
ProcessAspectChain().Invoke(event, [&] {
    stageResult = stage->Run(state);
    event.Failed = !stageResult.Ok;
    event.Detail = stageResult.Diagnostics;
});
```

`Detail` 是 `string_view`，不能指向 `Run` 返回后就销毁的临时字符串。把 `Diagnostics` 留在 `stageResult` 里，`event.Detail` 指向 `stageResult.Diagnostics`，并且 `Invoke` 返回之前不要让 `stageResult` 离开作用域。

`LoadXl` / `SaveXl` 的 `Subject` 用路径的 `filename()` 字符串，同样要活过 `Invoke`。`TessellateBody` 的 `Subject` 为空。`Regenerate` 的 `Subject` 用 Part 名字；没有名字就空。

不要在 `IFeature::Rebuild` 的每个子类里再包。再生只有 `Part::Regenerate` 这一处。

- [ ] **Step 4: 跑新测试和一条现有布尔测试**

```powershell
ctest --test-dir E:\brepkernelstudy\cmake-build-mingw-debug -R "brep_test_aspect|BooleanLock" --output-on-failure
```

Expected: PASS。几何断言不变。

- [ ] **Step 5: Commit**

```powershell
git add kernel/src/Part.cpp kernel/src/bool/Pipeline.cpp kernel/src/io/XlDocument.cpp kernel/src/mesh/Mesh.cpp tests/kernel/TestAspect.cpp
git commit -m "feat: trace kernel boundaries through the process aspect chain"
```

---

### Task 4: 场景门面

**Files:**
- Modify: `apps/viewer/adapter/SceneAdapter.cpp`
- Modify: `apps/viewer/tests/TestSceneAdapter.cpp`

**Interfaces:**
- Consumes: `ProcessAspectChain`

- [ ] **Step 1: 写失败测试**

在 `TestSceneAdapter.cpp` 用 `ScopedProcessAspectChain` 加记录切面。调用 `AddPrimitive` 造一个盒子。记录里有 `scene.addPrimitive`。`Subject` 是该特征的 id 文本或规格里的名字，两种都允许，但一次调用只有一条 `scene.addPrimitive`。

- [ ] **Step 2: 跑 `viewer_adapter_tests` 里这个用例，确认失败**

- [ ] **Step 3: 包裹规格第 5 节列出的八个 `SceneAdapter` 方法**

每个公开方法一个 `Site`，字符串与规格表一致。`Invoke` 包住现有函数体，不改返回的 `Body*`。`UndoFeature` / `RedoFeature` 的 `Site` 为 `scene.undo` / `scene.redo`。

只读方法（`MeshForBody`、`ObjectForFeature`、`SpecFor`）不包。

- [ ] **Step 4: 跑 `TestSceneAdapter`**

Expected: PASS。

- [ ] **Step 5: Commit**

```powershell
git add apps/viewer/adapter/SceneAdapter.cpp apps/viewer/tests/TestSceneAdapter.cpp
git commit -m "feat: trace scene mutations through the process aspect chain"
```

---

### Task 5: 窗口链与命令连接点

**Files:**
- Create: `apps/viewer/commands/CommandPayload.h`
- Modify: `apps/viewer/commands/CommandManager.h`
- Modify: `apps/viewer/commands/CommandManager.cpp`
- Create: `apps/viewer/commands/StatusAspect.h`
- Create: `apps/viewer/commands/StatusAspect.cpp`
- Modify: `apps/viewer/CMakeLists.txt`（`StatusAspect.cpp` 进入 `viewer_runtime`）
- Create: `apps/viewer/tests/TestCommandAspects.cpp`
- Modify: `apps/viewer/tests/CMakeLists.txt`（`viewer_command_tests`，链接 `viewer_bootstrap` 与 `GTest::gtest_main`）

**Interfaces:**
- Consumes: `AspectChain`、`CommandPayload`
- Produces: `CommandManager` 持有 `AspectChain`；`StatusAspect`

- [ ] **Step 1: 写失败测试**

`TestCommandAspects.cpp` 直接构造 `CommandManager(registry, chain)`。链上放两个记录切面，记录 `Before/After/OnError`、`Site`、`Failed`、`AnnounceCancel`。

覆盖规格第 8 节：

1. 瞬时命令 `command.run`：`Before` 正序，`After` 逆序，`Payload` 里的 `Result->Status` 为 `Ok`。
2. `execute` 抛 `std::runtime_error`：逆序 `OnError`，没有 `After`，异常仍在。
3. 返回 `Failed`：有 `After`，`event.Failed` 为真，没有 `OnError`。
4. 空链时 `run` 仍返回命令自己的 `CommandResult`。
5. 未知 id 与 `can_execute == false`：记录为空。
6. 交互工具 `OnStart` 后立即结束：先 `command.tool.start`，再 `command.tool.finish`，没有 `command.run`。
7. `OwnsUndoRedo` 工具上的 `edit.undo`：窗口链记录数不增加。
8. `make_tool` 为空：记录为空，结果 `Failed`。
9. `tool_mouse_move` 不增加记录。
10. `OnCancel` 不结束工具：四条记录都是 `command.tool.cancel`，`After` 带 `AnnounceCancel`。这个测试要先建一个函数局部静态 `QApplication`，因为取消会问活动弹出控件。

Status 单独一条：`ReportStatus` 对 `Ok("saved")` 收到一次 `"saved"`；空消息不调用；取消文案为 `已取消: ` 加工具 id。

- [ ] **Step 2: 编译 `viewer_command_tests`，确认失败**

- [ ] **Step 3: 实现**

`CommandManager` 构造函数接收 `AspectChain`。`run` 在通过前置检查后：

```cpp
AspectEvent event;
event.Site = "command.run";
event.Subject = cmd->id();
CommandPayload payload;
payload.ReportStatus = ctx.ReportStatus ? &ctx.ReportStatus : nullptr;
event.Payload = &payload;
return m_chain.Invoke(event, [&] {
    CommandResult result = cmd->execute(ctx);
    payload.Result = &result;
    event.Failed = result.Status == CommandStatus::Failed;
    event.Detail = ...; // 指向活过 Invoke 的 QString 缓冲区，用 payload 里的 QByteArray 成员存放 utf8
    return result;
});
```

`Detail` 用 `CommandPayload` 里一个 `QByteArray DetailUtf8` 成员保存，`event.Detail` 指向它的 `constData()`。在 `Invoke` 返回前不要清空。

工具的 `start` / `finish` / `cancel` 用规格第 8 节的 `Site`。`Prompt` 与 `AnnounceCancel` 在 `body` 末尾写入 `payload`，因为 `event` 按引用传入 `Invoke`。`finish` 在 `m_activeTool.reset()` 之前读取 `Id()`。

`StatusAspect::After` / `OnError` 按规格第 8 节的表调用 `ReportStatus`。其它 `Site` 忽略。

删除 `CommandManager` 里这些日志和状态栏调用：`command execute`、`tool started`、`tool finished`、`tool finished immediately`、`tool cancel`，以及对应的 `ReportStatus`。保留 `close popup` 和 `tool context menu`。

- [ ] **Step 4: 跑 `viewer_command_tests`**

Expected: PASS。

- [ ] **Step 5: Commit**

```powershell
git add apps/viewer/commands/CommandPayload.h apps/viewer/commands/CommandManager.h apps/viewer/commands/CommandManager.cpp apps/viewer/commands/StatusAspect.h apps/viewer/commands/StatusAspect.cpp apps/viewer/CMakeLists.txt apps/viewer/tests/TestCommandAspects.cpp apps/viewer/tests/CMakeLists.txt
git commit -m "feat: run viewer commands through a per-window aspect chain"
```

---

### Task 6: 启动时安装

**Files:**
- Modify: `apps/viewer/bootstrap/ViewerRuntimeServices.cpp`
- Modify: `apps/viewer/bootstrap/ApplicationContainer.cpp`（容器建成后调用 `InstallProcessAspects()`）
- Modify: `apps/viewer/tests/TestCommandAspects.cpp`
- Modify: `docs/superpowers/specs/2026-09-23-viewer-command-aspects-design.md`（状态改为「已实施」）
- Modify: `docs/architecture/adr/0012-viewer-command-aspects.md`（状态改为「已接受（已实施）」）

**Interfaces:**
- Consumes: `InstallProcessAspects`、`AspectChain::Add`、四个切面的构造
- Produces: `CreateCommandManager` 的窗口链顺序 `Timing`、`Logging`、`Error`、`Status`

- [ ] **Step 1: 写失败测试**

`CreateCommandManager(nullptr)` 的 `Names()` 为上述四项。再 `run("test.ok")`：trace 里 `command execute` 风格的 `command.run` 只出现一次，`elapsed_ms=` 只出现一次，`ReportStatus` 只被调用一次。

- [ ] **Step 2: 跑测试，确认名字列表为空或不匹配**

- [ ] **Step 3: 安装**

`CreateCommandManager` 造一条链，依次 `Add` 四个 **新实例**（不要把进程链里的指针搬过来）。`ApplicationContainer` 在 `build()` 成功返回前调用 `InstallProcessAspects()`。不要在 `ViewerRuntimeModule::RegisterServices` 里注册切面。

- [ ] **Step 4: 验收**

```powershell
ctest --test-dir E:\brepkernelstudy\cmake-build-mingw-debug -R "brep_test_aspect|viewer_command_tests|TestSceneAdapter|BooleanLock" --output-on-failure
```

在 `kernel/include` 与 `kernel/src` 搜索 `CommandPayload` 和 `Hypodermic`，切面相关新文件中应为零匹配。`Hypodermic` 本来就不应出现在 `kernel/`。

Expected: 上述测试 PASS。

- [ ] **Step 5: Commit**

```powershell
git add apps/viewer/bootstrap/ViewerRuntimeServices.cpp apps/viewer/bootstrap/ApplicationContainer.cpp apps/viewer/tests/TestCommandAspects.cpp docs/superpowers/specs/2026-09-23-viewer-command-aspects-design.md docs/architecture/adr/0012-viewer-command-aspects.md
git commit -m "feat: install process and per-window aspect chains at startup"
```
