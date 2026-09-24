# ADR 0012: 工程边界使用统一拦截器切面

**状态**：已接受（已实施）  
**日期**：2026-09-23  
**修订**：2026-09-23 — 范围从 Viewer 命令总线扩大到整仓边界。命令总线仍是连接点之一，不再是唯一连接点。  
**前置**：[ADR 0007](0007-hypodermic-ioc-adoption.md)、[layering.md](../layering.md)  
**规格**：[2026-09-23-viewer-command-aspects-design.md](../../superpowers/specs/2026-09-23-viewer-command-aspects-design.md)

## 背景

日志、耗时和失败记录散落在命令调度、场景门面、特征再生、布尔阶段、网格化和 `.xl` 读写上。只改 `CommandManager` 会留下第二套横切写法，内核路径仍然手写 `BREP_INFO`。

这些路径的算法本身不负责计时和审计。它们的共同点是「进入一层边界、离开一层边界」。

## 决策

1. **切面运行时放在 `brep_base`。** 类型是 `brep::IAspect` 与 `brep::AspectChain`。不引入 AOP 编译器，不链接 Hypodermic，不依赖 Qt。
2. **只在层边界织入，不在算法内部织入。** 连接点是命令生命周期、`ISceneService` 的修改操作、`Part::Regenerate`、`BooleanPipeline::Evaluate` 的每个阶段、`TessellateBody`、`LoadXl` / `SaveXl`。鼠标移动、键盘、Vulkan 帧、单个面的分类、单个三角形不设连接点。
3. **两条链，同一种类型。** 内核边界使用进程链 `ProcessAspectChain()`。每个窗口的 `CommandManager` 另持一条链，上面多一个需要 Qt 的 Status 切面。内核代码不引用窗口链。
4. **切面只观察。** 不改模型，不改返回值，不取消操作，不代替 `DocumentHistory` 或特征撤销。异常通知 `OnError` 后原样抛出。
5. **默认切面是 Logging、Timing、Error。** 三者放在进程链，也放在窗口链。Status 只放在窗口链。都不注册成 Hypodermic 单例。
6. **IoC 只负责安装。** `build_application_container` 之后安装进程链。`CreateCommandManager` 安装窗口链。执行路径不再 `resolve`。
7. **嵌套是预期行为。** 一条命令内部会再进入场景门面、再生和布尔阶段。外层与内层各记各的 `Site`，不合并成一条日志。

## 理由

- 运行时放在 `brep_base` 后，内核与 Viewer 共用顺序规则（`Before` 正序，`After` / `OnError` 逆序），又不必让内核看到 Qt 或 Hypodermic。
- 进程链解决「内核 API 不能为了切面全部加参数」。窗口链解决「计时和状态栏跟着窗口走」。
- 布尔 Pipeline 已经有阶段对象。切面包住 `IPipelineStage::Run`，不替换阶段本身。

## 后果

- **做**：`brep_base` 的链、上述边界上的 `Invoke`、进程链与窗口链的安装、对应测试。
- **不做**：按函数插桩的编译期织入、热循环里的连接点、把撤销改成切面、删掉各 Tool 内部已有的诊断日志。
