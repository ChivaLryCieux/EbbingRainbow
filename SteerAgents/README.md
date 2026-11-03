# SteerAgents

> AI 操作 UE5.8 的可复用工具包。当前形态是**外部 Python 客户端库**，
> 通过 UE 自带的 Model Context Protocol (MCP) 插件驱动运行中的编辑器。
> 长期目标：封装成一个可上架 Fab 的 UE 插件（见 `Docs/DESIGN.md`）。

## 结构

```
SteerAgents/
  client/steeragents/   外部 Python 包（import steeragents as sa）
    config.py           自动探测 .uproject / steeragents.toml / 环境变量
    mcp.py              MCP 会话（initialize / tools/list / tools/call）
    toolsets.py         connect / call / describe / text_of
    console.py          通过输出日志 Cmd 框执行控制台命令、`py` 脚本
    capture.py          CaptureViewport 截图 + 亮度统计
    __main__.py         CLI（见下）
  Docs/DESIGN.md        顶层设计与 Fab 路线
```

项目专属流程脚本仍在 `<proj>/Scripts/`，通过同名的 `mcp_call.py /
mcp_admin.py / ue_shot.py / ue_cmd.py` shim 透明转发到这里——老脚本
不需要修改。

## 用法

```powershell
$env:PYTHONPATH = "D:\EbbingRainbow\SteerAgents\client"   # 或加进系统环境变量

python -m steeragents toolsets                          # 列出所有 toolset
python -m steeragents list EditorToolset.EditorAppToolset
python -m steeragents call <toolset> <tool> '{"k":1}'
python -m steeragents cmd "ER.ArchDump 1"               # 控制台命令
python -m steeragents shot arena.png                    # 截图 + 亮度统计
```

Python 侧：

```python
import steeragents as sa
m = sa.connect()
sa.call(m, "EditorToolset.EditorAppToolset", "CaptureViewport", {...})
sa.capture(m, "arena.png")
sa.run_cmd(m, "ER.AutoShot 1")
```

## 配置

按优先级：环境变量 → 项目根的 `steeragents.toml` → 默认值。
`.uproject` 会从 cwd 向上自动探测，路径一律不写死。

| 环境变量 | toml | 默认 |
| --- | --- | --- |
| `STEER_MCP_URL` | `mcp.url` | `http://127.0.0.1:8000/mcp` |
| `STEER_SHOT_DIR` | `shots.dir` | `<proj>/Saved/Screenshots` |
| `STEER_EDITOR_CMD` | `engine.editor_cmd` | — |
| `STEER_BUILD_BAT` | `engine.build_bat` | — |
| `STEER_QUIET` | — | 关闭会话日志 |

## 前置条件

- UE 编辑器正在运行，且工程启用了 `ModelContextProtocol` 插件
  （本项目 `.uproject` 里已开）。
- 外部 Python 3.11+（仅用标准库；`stats()` 另需 Pillow）。
