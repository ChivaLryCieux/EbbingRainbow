# SteerAgents 顶层设计

定位：**让 AI agent 能看、能改、能验证 UE5.8 工程**。卖点不是"又一个
MCP server"（Epic 已经自带），而是把"操作编辑器 → 看结果 → 迭代"这条
闭环做成开箱即用的工作流，尤其面向美术/TA/独立开发者。

## 一、调研结论（UE5.8 官方 MCP 栈）

引擎自带三层，全部是 Experimental 插件：

- `ModelContextProtocol`：HTTP JSON-RPC server（默认
  `127.0.0.1:8000/mcp`），会话经 `Mcp-Session-Id` 头维持。
- `ToolsetRegistry`：工具注册框架。
  - C++：`UCLASS : UToolsetDefinition` + `UFUNCTION(meta=(AICallable))`，
    doc comment 自动进 JSON Schema（`ToolTip`/`ClampMin` 等 meta 也进）。
  - Python：在编辑器内 `import toolset_registry`，
    `@toolset_registry.tool_call` 装饰静态方法；Python toolset 支持
    `list/dict/set/Optional/ustruct` 注解。
  - 已注册 toolset 通过 `list_toolsets` / `describe_toolset` /
    `call_tool` 三个元工具统一调用。
- `AllToolsets` + 30 个官方 Toolset：EditorToolset（含 `CaptureViewport`
  视口截图、`StartPIE`）、SlateInspector（UI 树 snapshot + Type）、
  PCG / Niagara / UMG / LiveCoding / AutomationTest / ConfigSettings 等。
- `UAgentSkill`（UDataAsset）：官方"技能"资产，`ListSkills / GetSkills /
  CreateSkill / UpdateSkill` 工具——Epic 的思路和我们 AGENTS.md /
  skill 化方向一致，SteerAgents 的技能格式可直接兼容。

**结论**：SteerAgents 不重写协议层，做它的上层——一个
`SteerAgentsToolset`（补充官方没有的工具）+ 外部 client 库 + 工作流。

## 二、目标架构

```
┌───────────────────────── agent (Devin / Claude / 任意 MCP 客户端)
│  高层工作流 skills：look-loop / build-smoke / material-audit ...
├───────────────────────── client/steeragents（外部 Python 库，本仓库）
│  会话、toolset 调用、控制台、截图、图像度量、headless 启动
├───────────────────────── SteerAgentsToolset（UE 插件内的自定义工具，TODO）
│  console.exec / shot.list / level.arena_probe / build.errors 等
├───────────────────────── Epic: AllToolsets + ToolsetRegistry
├───────────────────────── Epic: ModelContextProtocol (HTTP server)
└───────────────────────── 运行中的 UnrealEditor / PIE
```

三条原则：

1. **插件内只做"官方 toolset 做不到或做得很差"的工具**（语义化的高层
   操作）；通用 CRUD 全部复用官方 toolset，少维护。
2. **client 库零项目耦合**：任何路径都从 `steeragents.toml` / 环境变量 /
   `.uproject` 自动探测来。这条是跨项目复用和上架 Fab 的前提。
3. **截图-度量-迭代闭环是一等公民**：`capture → stats/diff → 参数调整`
   做成固定 API，这是 SteerAgents 区别于裸 MCP 的核心价值。

## 三、对外形态（Fab 路线）

| 阶段 | 形态 | 说明 |
| --- | --- | --- |
| 0（现在） | 项目内 `SteerAgents/` 目录 | client 库随工程演进打磨 |
| 1 | 独立 `SteerAgents.uplugin` | `SteerAgentsToolset` C++ 模块（Editor only），
      `Content/Python/` 带 Python toolset；client 库以 `Resources/` 或
      附带 wheel 形式打包 |
| 2 | Fab 上架 | 需要：稳定到非 Experimental 依赖、英文文档、
      示例项目、明确比官方 MCP 多出的价值主张 |

风险与对策：

- **官方栈仍是 Experimental**：API 可能在 5.9/5.10 变。对策：SteerAgents
  只经 `ToolsetDefinition` 公共接口注册，不碰私有头；README 标注
  supported engine versions。
- **和官方 MCP 撞车**：Fab 审核会问"为什么不用自带的"。卖点必须落在
  工作流（视觉迭代、自动截图报告、生成器验收）而不是传输层。
- **EditorOnly**：SteerAgentsToolset 标 `EditorOnly + TargetAllowList
  [Editor]`，不打进游戏包，无运行时负担。

## 四、本工程内的落地

- `client/steeragents/` — 外部库（本目录已建）。
- `Scripts/` shim — `mcp_call.py / mcp_admin.py / ue_shot.py / ue_cmd.py /
  ue.py` 转接到包，老脚本不改。
- 待做：`SteerAgentsToolset` UE 插件目录（`Plugins/SteerAgents/`），先放
  `console.exec`（替代 SlateInspector 找 Cmd 框的脆弱方案）、
  `arena.regen`（一键重新生成 + navmesh 校验 + 报告）。
