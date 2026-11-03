# AGENTS.md — Ebbing Rainbow (退潮之虹)

科幻肉鸽回合制战棋（UE 5.8）：3D WFC 程序化战场 + 种子化 DAG 航线图 +
卡牌/AP 战术博弈。先美后好玩；画风目标 = 绝境北地/控制/Blackshard：
静谧、庄严、有神性。详见 `README.md`。

## 工程命令

```powershell
# 编译 C++（编辑器需关闭；本工程习惯全量构建而非 Live Coding）
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" EbbingRainbowEditor Win64 Development `
    -Project="D:\EbbingRainbow\EbbingRainbow.uproject" -WaitMutex

# 开编辑器（前置：WFC 材质脚本如需重跑先跑脚本再开编辑器）
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" D:\EbbingRainbow\EbbingRainbow.uproject
```

## 操作运行中的编辑器（SteerAgents）

`SteerAgents/` 是独立的可复用客户端包（长期做成 UE 插件上架 Fab，见
`SteerAgents/Docs/DESIGN.md`）。用之前确认编辑器开着（MCP server 在
`127.0.0.1:8000/mcp`）：

```powershell
$env:PYTHONPATH = "D:\EbbingRainbow\SteerAgents\client"
python -m steeragents toolsets            # 可用 toolset
python -m steeragents cmd "ER.ArchDump 1" # 控制台命令
python -m steeragents shot arena.png      # 截图到 Saved/Screenshots
```

`Scripts/` 里的 `mcp_call.py / mcp_admin.py / ue_shot.py / ue_cmd.py /
ue.py` 是转发到 SteerAgents 的 shim——**新代码直接用 `steeragents`，
别再往 Scripts/ 加 `from mcp_call import ...`**。

## 画面迭代闭环（最重要）

改画面相关代码后必须走完这条环，不许凭感觉收尾：

1. 编译 → 启动/重进 `Lvl_Arena`（或 `ER.AutoShot 1` 走完整流程）；
2. `python -m steeragents shot <名>.png` 拿俯视/战术两个机位；
3. **看图说话**：对照参考画风评估构图、留白、焦点、雾；
4. `sa.stats()` 亮度分带：近黑比例、条带均值是否拉开层次；
5. 不达标就回到 1。

控制台调试命令：`ER.ArchLayout -1|0..6`（-1=种子自动选原型）、
`ER.ArchDump 1`（ASCII 平面图进日志）、`ER.AutoShot 1`（竞技场就绪后
自动切俯瞰相机截图）、`RouteDump`、`ER.AutoWin/AutoPlay`、`Paint.Style`。

## 代码约定与已知坑

- 战斗/WFC 纯 C++，UI 是 Slate（`S` 前缀类）；`AGENTS.md` 不存在的规矩
  看相邻文件即可。新增 WFC 风格 = 新 `WFC3DArenaGenerator_*` + StyleTable
  记录，不动 Host/Solver。
- `Content/WFCKit/Materials` 的 `.uasset` 可能是**只读**，保存会静默失败；
  脚本先清只读位（参考 `wfc_apply_futuristic_materials.py`）。
- `WFCKitTileTable.h` 由 `Scripts/gen_tile_table.py` 生成，勿手改。
- 瓦片 SM 材质槽固定指向 `MI_WFC_*`；换皮只改 MI，别动生成器/碰撞。
- Painterly 后处理材质靠字符串引用，目录已在
  `DirectoriesToAlwaysCook`；挪目录要同步 `DefaultGame.ini`。
- 楼梯碰撞用轴对齐台阶盒（旋转斜坡 navmesh 烘不出）；上层楼板 +4 m，
  台阶顶 416 cm 与回廊楼板齐平。
- 生成必须**种子可复现**：同 seed 同图，派生子 seed 用 `Seed + N*k`
  不要用全局 `FMath::Rand()` 污染。

## 美术方向验收（当前画面问题清单）

- 不要盒子感：外圈向界外的墙正在改成低女儿墙/破碎边，做"漂浮沙盘"。
- 细节要有主次：地面瓦片按空间成区而不是逐格随机；发光条/道具密度
  受控（当前的 22% floor_light、34% prop 太吵）。
- 光照要有单一主光 + 体积雾 + 留白（深渊/云海），拒绝均匀粉紫环境光。
- 相关设计意图随时更新本节。
