# AGENTS.md — Ebbing Rainbow (虹不我与)

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

SteerAgents 已拆出为**独立仓库 `D:\SteerAgents`**（UE 插件 + Python
客户端，见 `D:\SteerAgents\Docs\DESIGN.md`；MIT）。本工程通过
junction `Plugins/SteerAgents -> D:\SteerAgents` 挂入插件（已在
`.uproject` 启用），客户端经 `Scripts/` shim 的探测链解析
（`STEERAGENTS_HOME` env → 兄弟目录 `client/src` → 已安装包）。
工程配置在 `steeragents.toml`（editor_cmd / build_bat 已配好）。

用之前确认编辑器开着（MCP server 在 `127.0.0.1:8000/mcp`）：

```powershell
python -m steeragents toolsets            # 可用 toolset
python -m steeragents cmd "ER.ArchDump 1" # 控制台命令（走插件 ExecConsoleCommand）
python -m steeragents shot arena.png      # 截图到 Saved/Screenshots
python -m steeragents exec-py "print(1)"  # 编辑器内跑 Python，拿回输出
python -m steeragents doctor              # MCP/插件/Pillow 体检
```

没装包就 `uv pip install -e D:\SteerAgents\client` 或设
`$env:PYTHONPATH = "D:\SteerAgents\client\src"`。

`Scripts/` 里的 `mcp_call.py / mcp_admin.py / ue_shot.py / ue_cmd.py /
ue.py` 是转发到 SteerAgents 的 shim——**新代码直接用 `steeragents`，
别再往 Scripts/ 加 `from mcp_call import ...`**。

## 画面迭代闭环（最重要）

改画面相关代码后必须走完这条环，不许凭感觉收尾：

1. 编译 → 启动/重进 `Lvl_ConcreteArena` 或 `Lvl_NatureArena`（或 `ER.AutoShot 1` 走完整流程）；
2. `python -m steeragents shot <名>.png` 拿俯视/战术两个机位；
3. **看图说话**：对照参考画风评估构图、留白、焦点、雾；
4. `sa.stats()` 亮度分带：近黑比例、条带均值是否拉开层次；
5. 不达标就回到 1。

控制台调试命令：`ER.ArchLayout -1|0..6`（-1=种子自动选原型）、
`ER.ArchDump 1`（ASCII 平面图进日志）、`ER.AutoShot 1`（竞技场就绪后
自动切俯瞰相机截图；`-AutoShotTop` 正俯机位）、`RouteDump`、
`RouteSelect N`（模拟节点点击，走与鼠标同一条路径）、
`ER.AutoWin/AutoPlay`、`ER.StatusSelfTest`（技能系统状态生命周期自检）、
`Paint.Style`。

命令行参数：`-ArenaSeed=N`（固定竞技场种子）、
`-ArenaGenerator=-1|0|2`（强制指定生成器，压过关卡场景档案；
平时**不传**——开哪张关卡就用哪种地貌）、`-AutoShotTop`、
`-RouteShot`（航线图 3D 场景搭好后自动截图）、`-ShowNav`（截图时显示导航）、
`-ArenaForceReady`（跳过 nav 校验仅截图）、`-NatureNavBox/-NatureCube`
（自然岛 nav 诊断）、`-Atmo`（显式开启程序氛围：种子化三点光+虚空雾；
默认关——生成器不生成灯也不调雾，关卡自摆光照归用户手调）。

## 场景架构（当前）

- `Lvl_Route`（空关卡，全部程序生成）：`ARouteSceneBuilder` 在
  `RouteGameMode::StartPlay` 生成，纯自发光、不依赖关卡灯光。
  布局常数/节点坐标/贝塞尔采样唯一来源 `Run/RouteLayout.h`（Slate 与 3D 共用）；
  3D 对齐**读 Slate 实际布局**（`ARouteHUD::GetMapLayout`），不要自己用公式推
  （Overlay 非对称 padding 会差十几像素），视口/布局变化自动重建；节点状态变化
  （安全节点前进）HUD 里再调 `BuildScene` 刷新。
  材质是程序化资产：`Scripts/route_make_materials.py`(Glow) + `route_make_materials2.py`(Nebula/Rainbow/Rays/Halo/Monolith，Python 搭材质图，`Power` 节点引脚叫 `Exp`、Noise 第一引脚叫 `World Position`，Power 底数为负会被钳 0) → `/Game/Run/Materials`（已在 `DirectoriesToAlwaysCook`）。**设计铁律（用户明确要求）：全场只有玩家当前所在节点发光，其余节点/连线一律不发光；不放零星粒子（试过 Niagara 星尘/火星，观感廉价，已删）；连线只用细实线；同层节点最小纵向间距见 `RouteLayout.h::MinLayerGap`。**BasicShapes/Plane 的 UV：u 沿局部 X、v 沿局部 Y。
  Slate 层（节点印记 `SRouteNodeSigil`、连线辉光+流光、层级标尺、图例）全是自绘，
  大范围辉光交给 3D 的 `M_Route_Halo`（Slate 叠圆会出台阶边）。
  截图迭代：`powershell -ExecutionPolicy Bypass -File Scripts/route_shot.ps1 [-Seed N]`
  （起 `-game` 进 Lvl_Route，等 `-RouteShot` 截图后关进程）。
  关卡没有自带无界 PostProcessVolume 时，builder 才会生成固定曝光+bloom+暗角的体积。
- **一场景一关卡**（战斗场景）：`Lvl_ConcreteArena` = 混凝土浮岛
  （默认体素生成器 `WFC3DArenaGenerator_Voxel`，Perlin 双频
  起伏+中块平台+破碎漂浮外壳，混凝土/大理石/玻璃三色+辉光渐变）；
  `Lvl_NatureArena` = 自然岛（`WFC3DArenaGenerator_Nature`，Gaea 高度图
  随机截取+Perlin 岛形，可玩区=宏观骨架（主坡/洼地）+150cm 梯田台地+细节，
  起伏调参见该生成器头文件 `Relief*` 常量，PMC 地形+HISM nav 代理层）。
  场景种类由**关卡名**
  声明（`BattleArena.cpp` 地图场景档案），航线 `Battle` 节点开前者、
  `BattleNature` 开后者；`-ArenaGenerator=N` 命令行可强制覆盖（调试），
  `?gen=N` URL 覆盖仅限调试。光照/雾/后处理各关卡各自手调，规则代码
  不按场景复制。
- 旧 `WFC3DArenaGenerator_Atrium`（语法+socket 铺瓦）保留不用。
- 航线节点 `Battle`→体素场，`BattleNature`→自然岛，遭遇种子来自节点。

## 代码约定与已知坑

- 技能/状态/战斗数值的顶层架构见 `Docs/SkillSystem-2.0.md`（技能系统 2.0）：
  统一 `UBattleSkillData` + `FBattleSkillSpec`（旧 `FBattleSkillDef` 已废除）、
  状态效果带持续回合（派生属性变化必须走状态，禁止瞬间改）、结算走
  `UBattleResolver` 管线（事件总线 `UBattleEventHub`）、战斗随机数种子化
  （`FBattleRandomStream`，禁止战斗代码用 `FMath::Rand`）、地形耦合查
  `UBattleTerrainQuery`（视线/掩体/高低差/侧翼）。新技能 = 数据/内置工厂
  （`UBattleSkillLibrary`）加条目，不写新类；学派×状态互动 = 反应表加行。
- 战斗/WFC 纯 C++，UI 是 UMG `UER*` 控件（内嵌 Slate leaf 自绘，旧 `S*Overlay`/`SRouteNodeWidget` 已删）；`AGENTS.md` 不存在的规矩
  看相邻文件即可。新增 WFC 风格 = 新 `WFC3DArenaGenerator_*` + StyleTable
  记录，不动 Host/Solver。
- **Source 目录布局**（2026-11 重组后）：`Core/`（ERLog 日志分类、
  ERDebug 统一开关、ERAssetCatalog 资产目录、ERCollisionChannels）、
  `Battle/`（Core=GameMode/Manager/UBattleContext/UBattleDebugSubsystem、
  Units、Skills、Terrain、AI=UBattleAIDirector、Player、Arena、UI、FX=飘字池）、
  `Run/`（航线图+RunSubsystem）、`Hall/`、`WFC/`（Host/Solver/StyleTable/
  Shared=ERArenaAtmosphere/Voxel/Nature）、`Rendering/`、`Legacy/`（Atrium+语法、
  Styles=旧 5 风格生成器、Motorcycle、ThirdPerson 模板、DayNight）。
  include 一律模块相对路径（`"Battle/Core/BattleManager.h"`），
  Build.cs 只挂模块根。旧实现的平铺文件名不再可用，引用前先看新位置。
- 战斗世界状态查 `UBattleContext`（单位注册表+Rng+Manager），**禁止再用
  `TActorIterator<ABattleUnit>`**；LOS 走 `ER_TRACE_BATTLELOS`
  （ECC_GameTraceChannel1，DefaultEngine.ini 注册名 BattleLOS），
  单位胶囊对该通道 Ignore、地形默认 Block，不要手写忽略列表。
- 调试自动化（AutoShot/AutoWin）在 `UBattleDebugSubsystem`，
  别往 GameMode 塞；调试命令/CVar 加 `#if !UE_BUILD_SHIPPING` 或 `ECVF_Cheat`。
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
- **Slate `SLATE_ARGUMENT` 的 POD 类型（指针/int/float/bool/枚举）必须在
  `SLATE_BEGIN_ARGS(...)` 的初始化列表里给默认值**：`FArguments` 不会
  清零，漏写 + 调用点没传 = 读到栈垃圾（曾因 `_IconTexture` 未初始化，
  移动卡拿到野指针，在 `FSlateImageBrush`/`SImage::OnPaint` 里崩成
  "Array index out of bounds"）。崩溃栈指向 Slate 绘制时，先查这类问题，
  别先怀疑 GC/纹理资源。
- **全场景 UMG 蓝图套件在 `Content/UI/`（C++ 基类在 `Source/EbbingRainbow/UI/`，`UER*` 前缀）**：
  所有界面均已落地为可直接在 UMG Designer 手调的可视化蓝图，全局入口统一在
  `Project Settings -> Game -> ER UI`（`UERUISettings` / `DefaultGame.ini`）：
  - 大厅 `Content/UI/Hall/`：`WBP_HallHUD` + `WBP_HallButton`（`AHallHUD` 挂载）；人员管理面板
    `WBP_PersonnelPanel` / `WBP_PersonnelMemberRow` / `WBP_PersonnelStatCell`（`AHallHUD::OpenPersonnelPanel`
    按需创建，入口在 `Project Settings -> Game -> ER UI -> PersonnelPanelWidgetClass`；生成脚本
    `Scripts/setup_personnel_panel.py`。四人档案数值唯一真相源仍是
    `ABattleGameMode::GetDefaultPlayerRoster()`，表现层在 `Hall/HallRoster.h`。立绘占位统一用根目录
    `IMG.PNG` → `/Game/UI/Textures/T_UI_HeroPortrait_Placeholder`，`UERPersonnelPanel::PlaceholderPortrait`
    是兜底软引用，专属立绘按 `FHallHeroProfile::Portrait` 覆盖）；
  - 航线图 `Content/UI/Route/`：`WBP_RouteHUD` + `WBP_RouteNode` + `WBP_RouteLegendItem`
    （矢量印记/贝塞尔连线/层级标尺封装为 Palette `"ER UI|Route"` 下的 `UWidget` 原子控件
    `UERRouteNodeSigil / UERRouteEdgeLayer / UERRouteLayerRuler / UERRouteTypeIcon`；
    `UERRouteHUD::GetMapLayout` 读 `NodeCanvas` 的真实几何与 3D `ARouteSceneBuilder` 对齐）；
  - 战斗 `Content/UI/Battle/`：`WBP_BattleHUD`、`WBP_UnitToken`（编队）、`WBP_InitiativeToken`（先攻）、
    `WBP_SkillCard`、`WBP_SkillTooltip`、`WBP_HeroPlate`、`WBP_PipBar`、`WBP_ActionButton`、
    `WBP_BattleResult`、`WBP_BattleResultRow`、`WBP_FloatingText` + `BP_BattleFloatingText`（跳字池）；
  - 主题与公共资产：`Content/UI/DA_ERUITheme`、`F_ERUI_Composite`、`T_UI_{White,Scrim,Ring}`；
  - 自动化构建/修复脚本：`Scripts/setup_ui_assets.py`（注意会重置新建子蓝图的默认树，手调后勿盲跑覆盖）。
  - UMG 已知死坑：C++ 兜底建树放 `RebuildWidget()`；构建期图片定尺寸写 `FSlateBrush::ImageSize`
    （UE5.8 Python 侧为 `unreal.DeprecateSlateVector2D`）；运行时不覆盖 Designer 手调的
    `ChipBox` 宽高与 `RenderTransform` 基准偏移；裸 `UFontFace` 不能直接赋给 `FontObject`，统一用
    `F_ERUI_Composite` 或 `UERUITheme::GetFont()`。

## 美术方向验收（当前画面问题清单）

- 不要盒子感：外圈向界外的墙正在改成低女儿墙/破碎边，做"漂浮沙盘"。
- 细节要有主次：地面瓦片按空间成区而不是逐格随机；发光条/道具密度
  受控（当前的 22% floor_light、34% prop 太吵）。
- 光照要有单一主光 + 体积雾 + 留白（深渊/云海），拒绝均匀粉紫环境光。
- 相关设计意图随时更新本节。
