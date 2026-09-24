# 工程边界切面

**日期**：2026-09-23  
**状态**：已实施  
**关联**：[ADR 0012](../../architecture/adr/0012-viewer-command-aspects.md)、[ADR 0007](../../architecture/adr/0007-hypodermic-ioc-adoption.md)、[layering.md](../../architecture/layering.md)

本文替换「只包 `CommandManager`」的范围。命令总线仍按第 8 节执行，它使用同一套 `AspectChain`。

---

## 1. 目的

用同一种拦截器描述整仓的横切行为：日志、耗时、失败记录，以及 Viewer 命令的状态栏提示。各层算法不负责这些事。

---

## 2. 范围

### 做

- 在 `brep_base` 提供 `IAspect`、`AspectEvent`、`AspectChain`。
- 进程链覆盖内核边界；窗口链覆盖命令生命周期。
- 默认切面 Logging、Timing、Error。窗口链另加 Status。
- 按下表织入，并补测试。

### 不做

- 新的 AOP 库、宏织入器、编译期修改调用点。
- 内核 include Hypodermic 或 Qt。
- 鼠标按下、移动、键盘、右键菜单、Vulkan 每帧、单个面分类、单个三角形。
- 用切面实现撤销，或改 `DocumentHistory`。
- 工具在 `OwnsUndoRedo` 时处理的 `edit.undo` / `edit.redo`。
- 各 Tool 源文件里已有的诊断 `BREP_INFO`。
- 把切面注册成 Hypodermic 单例。
- 让切面修改返回值或中止调用。

---

## 3. 运行时

头文件 `kernel/include/brep/Aspect.h`，由 `api/Base.h` 转出（与 `Log.h` 相同）。实现在 `kernel/src/base/Aspect.cpp`，编进 `brep_base`。

```cpp
struct AspectEvent
{
    std::string_view Site;
    std::string_view Subject;
    bool Failed{false};
    std::string_view Detail;
    const void* Payload{nullptr};
};

class IAspect
{
 public:
    virtual ~IAspect() = default;
    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    virtual void Before(const AspectEvent& event) = 0;
    virtual void After(const AspectEvent& event) = 0;
    virtual void OnError(const AspectEvent& event, const std::exception& error) = 0;
};

class AspectChain
{
 public:
    void Add(std::unique_ptr<IAspect> aspect);
    [[nodiscard]] std::vector<std::string> Names() const;

    template <class F>
    decltype(auto) Invoke(AspectEvent& event, F&& body);
};
```

`Invoke` 的顺序：

1. 已注册切面正序 `Before`。
2. 执行 `body`。
3. 逆序 `After`。

`Before` 或 `body` 抛出 `std::exception` 时：只对 **已经返回** 的 `Before` 逆序调用 `OnError`，不调用 `After`，然后原样抛出。`After` 或 `OnError` 自己抛出时，不再调用剩余切面。

`body` 可以在返回前改 `event`（例如 `Failed`、`Detail`、`Payload`）。`After` 看到的是改过的事件。`Invoke` 不根据 `Failed` 去调用 `OnError`。`Failed` 只表示业务失败，仍走 `After`。

空链调用 `body` 并返回其结果。

---

## 4. 两条链

| 链 | 谁持有 | 装什么 | 谁调用 |
|----|--------|--------|--------|
| 进程链 | `ProcessAspectChain()`，定义在 `brep_base` | Timing、Logging、Error | 第 5 节的内核与场景边界 |
| 窗口链 | 该窗口的 `CommandManager` | Timing、Logging、Error、Status | 第 8 节的命令连接点 |

`ScopedProcessAspectChain` 在构造时换上一条临时进程链，析构时换回。测试用它，生产代码不用。

进程链是进程级观察器，不允许存放「当前窗口」的 Qt 对象。Status 只存在于窗口链。

同一条用户操作会嵌套多条事件。例如布尔命令：

```text
command.run / command.tool.*     窗口链
  scene.addBoolean               进程链
    part.regenerate              进程链
      boolean.pipeline / Select  进程链
      mesh.tessellate            进程链
```

每层写自己的 `Site`。不在内层抑制外层，也不在外层吞掉内层。

---

## 5. 连接点目录

`Subject` 是这条事件的名字（命令 id、阶段名、特征 id、文件名）。没有合适名字时用空字符串。`Payload` 只有命令事件使用，其余为 `nullptr`。

| Site | 包裹 | 位置 |
|------|------|------|
| `part.regenerate` | `Part::Regenerate()` | `kernel/src/Part.cpp` |
| `boolean.pipeline` | `BooleanPipeline::Evaluate` 里每一次 `IPipelineStage::Run`。`Subject` 为 `PipelineStageName` | `kernel/src/bool/Pipeline.cpp` |
| `io.xl.load` | `LoadXl` | `kernel/src/io/XlDocument.cpp` |
| `io.xl.save` | `SaveXl` | 同上 |
| `mesh.tessellate` | `TessellateBody` | `kernel/src/mesh/Mesh.cpp` |
| `scene.addPrimitive` | `SceneAdapter::AddPrimitive` | `apps/viewer/adapter/SceneAdapter.cpp` |
| `scene.addBoolean` | `SceneAdapter::AddBoolean` | 同上 |
| `scene.addExtrude` | `SceneAdapter::AddExtrudePad` | 同上 |
| `scene.duplicate` | `SceneAdapter::DuplicateBody` | 同上 |
| `scene.transform` | `SceneAdapter::TransformBody` | 同上 |
| `scene.removeFeature` | `SceneAdapter::RemoveFeature` | 同上 |
| `scene.undo` / `scene.redo` | `UndoFeature` / `RedoFeature` | 同上 |
| `command.run` | 瞬时命令与视图命令的 `execute` | `CommandManager` |
| `command.tool.start` | `OnStart` | 同上 |
| `command.tool.finish` | 工具结束后的收尾 | 同上 |
| `command.tool.cancel` | `OnCancel` | 同上 |

场景事件走进程链，因为 `SceneAdapter` 没有窗口链指针，而且它的调用方不只有 `CommandManager`。

以下失败不进切面，调用方拿到的结果与现在相同：

- 未知命令 id
- `can_execute` 为假
- 交互命令的 `make_tool` 返回空

`OwnsUndoRedo` 为真时，`edit.undo` 与 `edit.redo` 不进窗口链。它们若改动了文档，内层的 `scene.undo` 或 `part.regenerate` 仍会照常出现。

---

## 6. 与 IoC

Hypodermic 不织入，只在启动时安装。

- `ViewerRuntimeModule` 继续只注册 `CommandRegistry` 单例。
- 应用容器建好之后、窗口打开之前，调用一次 `InstallProcessAspects()`，向进程链加入 Logging、Timing、Error。重复调用先清空再安装，避免测试或二次启动叠两份。
- `CreateCommandManager` 为该窗口 `Add` 四份新的切面实例。不把窗口链放进容器。
- `ICommand::execute`、`IFeature::Rebuild`、`Part::Regenerate` 内部不 `resolve`。

---

## 7. 默认切面的日志

三者都通过 `BREP_INFO` 或 `BREP_ERROR` 输出。测试用 `SetAspectTraceSinkForTest` 拿到同一条已经格式化的消息。生产环境不设置这个函数指针。内核日志仍写进 `Log.cpp` 里的私有 logger。

| 切面 | 何时 | 消息里至少包含 |
|------|------|----------------|
| Timing | 对应 `After` 或 `OnError` | `Site`、`Subject`、`elapsed_ms=`。同一事件只记一次 |
| Logging | `After` | `Site` 与 `Subject` |
| Error | `After` 且 `Failed`，或 `OnError` | `Site`、`Subject`、`Detail` 或 `exception.what()`，级别为 `BREP_ERROR` |

`Failed == true` 不是异常。Error 在 `After` 里识别它。

阶段失败时，`BooleanPipeline::Evaluate` 在 `Run` 返回后把 `event.Failed` 设为 `!stageResult.Ok`，`Detail` 设为 `Diagnostics`，然后再离开 `Invoke`。不要为了失败再抛一个异常。

---

## 8. 命令事件

窗口链上的命令切面如果要读 Qt 数据，把 `Payload` 转成 `CommandPayload`（定义在 `apps/viewer/commands/CommandPayload.h`，不进内核）：

```cpp
struct CommandPayload
{
    const CommandResult* Result{nullptr};
    QString Prompt;
    const std::function<void(const QString&)>* ReportStatus{nullptr};
    bool AnnounceCancel{false};
};
```

`command.tool.start` 的 `body` 在 `OnStart` 返回后填写 `Prompt`。工具已经结束时 `Prompt` 为空。  
`command.tool.cancel` 的 `body` 在 `OnCancel` 返回后，仅当工具未结束且调度深度为 0 时把 `AnnounceCancel` 设为真。一次取消只有一个 `command.tool.cancel`。若取消导致工具结束，随后再有一个 `command.tool.finish`。

Status（`Name()` 为 `"Status"`）只看 `Site` 以 `command.` 开头的事件：

| 时机 | 文案 |
|------|------|
| `command.tool.start` 的 `After`，`Prompt` 非空 | `Prompt` |
| `command.run` 或 `command.tool.finish` 的 `After`，`Result->Message` 非空 | 该消息 |
| `command.tool.cancel` 且 `AnnounceCancel` | `已取消: {Subject}` |
| 命令事件的 `OnError` | 异常的 `what()` |

`ReportStatus` 为空则跳过。Status 不写日志。Error 不调用 Status。

从 `CommandManager` 去掉已经由窗口链负责的 `BREP_INFO` 和 `ReportStatus`。保留弹出控件关闭，以及 `tool context menu` 那条日志。`m_toolDispatchDepth > 0` 时仍不销毁工具。

---

## 9. 测试与验收

- `brep_test_aspect`：空链、正序 `Before`、逆序 `After`、异常时逆序 `OnError` 且没有 `After`、`Failed` 走 `After`。
- 进程链：`ScopedProcessAspectChain` 离开作用域后恢复原链。
- `BooleanPipeline` 一次求值至少产生六个 `boolean.pipeline` 事件，`Subject` 依次为六个阶段名。阶段返回 `Ok == false` 时该事件 `Failed`。
- `Part::Regenerate`、`LoadXl`、`SaveXl`、`TessellateBody` 各有一条事件。
- `viewer_command_tests` 覆盖第 8 节：顺序、立即结束的工具、撤销跳过窗口链、空工具、鼠标移动不是连接点、取消时 `AnnounceCancel`、Status 只回调一次。
- `CreateCommandManager(nullptr)->` 窗口链名字顺序为 Timing、Logging、Error、Status。
- `kernel/` 中除 `Aspect.h` / `Aspect.cpp` 外，不出现 `QString`、Hypodermic、`CommandPayload`。
- 现有 ctest 保持通过。嵌套事件允许变多，不允许同一条 `command execute` 文本出现两次。
