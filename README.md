# Ebbing Rainbow (虹不我与)

> **科幻大兵团军事指挥模拟游戏 (Sci-Fi Galactic Military Command Simulation)** · UE 5.8  
> 玩家扮演**虹膜基金会 (Iris Foundation)** 研究员，通过虹膜神经接驳技术将战术 AI 接入前线单位，围绕 **「我们面向全银河提供军事指挥服务」** 承接泰拉帝国 / 自由叛军 / 边缘自治领委托，在大尺度起伏地形上指挥多支兵团展开类《绝境北地》式即时战术对抗并改写银河战局。  
> *(仓库同时保留早期 3D WFC 程序化战场、种子化 DAG 航线图与 4v4 回合制卡牌原型代码。)*

---

## 🛸 新策划案核心架构：虹膜基金会银河军事指挥（当前主线）

```
                ┌──────────────────────────────────────────────────────┐
                │   虹膜基金会总署大厅 (Lvl_Hall / WBP_HallHUD)         │
                │   口号:「我们面向全银河提供军事指挥服务」             │
                │   左部标题下方纵向四按钮主面板:                       │
                │   [ 01 兵团整备 ] [ 02 银河委托 ]                    │
                │   [ 03 银河局势 ] [ 04 设置 ]                        │
                └───────────────────────┬──────────────────────────────┘
                                        │ 四按钮直达同一全屏终端的对应页签
                    ┌───────────────────┼────────────────────┐
                    ▼                   ▼                    ▼
     ┌────────────────────┐ ┌────────────────────┐ ┌────────────────────┐
     │ 虹膜兵团整备页      │ │ 银河委托页          │ │ 银河局势页          │
     │ - 4~8 支常备兵团战损│ │ - 横向三张合同卡片  │ │ - 三方阵营控制度    │
     │ - 经费补员/扩编    │ │ - 接入虹膜·出击部署 │ │ - 6 星区归属/战报   │
     │ - 虹膜AI同步升级   │ │                    │ │                    │
     └────────────────────┘ └─────────┬──────────┘ └────────────────────┘
                                      │ (设置页为占位终端)
                                      ▼
                                                    ┌──────────────────────────────┐
                                                    │  大兵团战术战场 (Lvl_Legion)  │
                                                    │  - PBR 抗锯齿等高线大地形     │
                                                    │  - 2D 坡度寻路 + 黄金角散阵   │
                                                    │  - 四兵种克制 + 左键射击/右键指挥│
                                                    │  - 左上黑白等高线雷达小地图   │
                                                    └──────────────┬───────────────┘
                                                                   │ 胜利 / 撤离结算
                                                                   ▼
                                                    写回 UGalacticCampaignSubsystem
                                                    (经费 / 战损 / 声望 / 银河控制率)
```

### 1. 宏观战役与双独立终端 (`Campaign/` & `UI/ERGalaCommandPanel.*`)
* **`UGalacticCampaignSubsystem`**（`GameInstanceSubsystem` 跨关卡持久化）：
  * **三大阵营**：泰拉帝国（`Empire`）、边缘自治领（`Dominion`）、自由叛军（`Rebels`），维护各自银河控制率（归一化和 100%）、基金会声望与 6 个星区归属；
  * **常备兵团编制 (`FManagedLegionData`)**：记录每支兵团呼号、兵种、战损剩余人数、`SyncLevel`（1~5 级虹膜 AI 同步加成）；战后需消耗经费补充伤亡兵员；
  * **动态合同 (`FGalacticContract`)**：每周期生成覆盖三方势力的委托，含敌方多兵种及延迟空降增援波次。
* **大厅入口 (`ERHallHUD` / `HallHUD`)**：
  * 主标题碑植入核心口号 **「我们面向全银河提供军事指挥服务」**；
  * 左部标题下方纵向排列四枚「白底墨字彩条」主控按钮：`01 兵团整备`、`02 银河委托`、`03 银河局势`、`04 设置`；四者打开同一全屏终端 (`UERGalaCommandPanel`) 的对应页签，终端内不设导航，`返回大厅` 回到主面板；
  * **银河委托页**：仅保留从左至右横向三等分的委托卡片（雇主→敌对→战区 / 简报 / 敌情与酬金 / 出击部署），卡片撑满页高；
  * **银河局势页**：三方阵营控制度与信条简报（左列）+ 6 大星区归属与上一战况（右列）；
  * **设置页**：占位终端（项目尚无 GameUserSettings 设置系统）。

### 2. 大兵团战场与《绝境北地》式操控 (`Experimental/LegionBattle/`)
* **多兵种克制与主动协议 (`LegionTypes.h` / `LegionUnit.*`)**：
  * **重盾卫队 (`SHD`)**：正面 ±63° 高额格挡减伤、惧侧背突袭；主动协议 **「铁壁共振」**；
  * **长戟方阵 (`PIK`)**：克制近战突击冲锋并施加击退；主动协议 **「拒马突刺」**；
  * **突击刃兵 (`VGD`)**：高机动破阵切后排；主动协议 **「虹膜突进」**；
  * **精确射手 (`MRK`)**：远程齐射弹道、高地射程延展（惧近身）；主动协议 **「高轨齐射」**。
* **自然散落编队与敌我统一动画**：
  * 放弃方格矩阵与刚性 3D 框线，改用**黄金角向日葵采样 + 扰动 + 4 轮最小间距排斥松弛**形成自然散落战团；
  * 旋转时兵团外框不整体甩动，改为每名士兵沿各自原地独立平滑转向 (`S.CurYaw`)；
  * 敌我双方统一采用 `SKM_Manny_Simple` 骨骼网格体、`MM_Idle` / `MF_Unarmed_Jog_Fwd` / `MM_Attack_*` / `MM_Death_*` 动画与 Ragdoll 物理击飞，通过动态材质 `"Paint Tint"` 区分阵营色。
* **轻量化地形寻路与黑白等高线小地图 (`LegionTerrainGrid.*` / `LegionHUD.*`)**：
  * `FLegionTerrainGrid` 开局采样地形生成 2D 坡度网格（`250 cm` 栅格），Grid A* + 视线拉直自动绕开 `>38°` 陡崖，并提供高低差伤害加成；
  * 运行时由高度场双线性合成黑白等高线图（`256` 小地图 / `1536` 全图 / `3072` 放大细节三档纹理，烘焙多线程并按分辨率放粗线宽；林地区域带斜纹暗影），严格对齐 `Yaw=90°` 方位；点击小地图或按 `M` 进入**等高线地图视角**（见下「3C」）。
* **3C：FP 模板第一人称指挥官 + 等高线地图双视角 (`FPS/LegionCommanderCharacter.*` / `LegionPlayerController.*`)**：
  * **战场视角（默认）**：玩家是一名第一人称指挥官（UE FP 模板射击栈 `ALegionCommanderCharacter`：手臂+手枪，`Variant_Shooter` 蓝图栈经 `TP_FirstPerson` 重定向挂载；Enhanced Input 驱动 WASD/视角/开火），WASD 移动、鼠标转向、`Shift` 疾跑、`空格` 跳跃；**左键纯射击**（仅地图开着/光标呼出时不打，命中敌团士兵由逐兵胶囊代理结算伤害，打中自己人无效）、**右键全权指挥**（点我方兵团选中/再点取消，`Ctrl` 增减选；点敌方兵团或番号集火；点地面机动，按住拖出阵型朝向，`F` 令兵团列队跟随）；按住 `Alt` 临时呼出光标点 HUD 或小地图；
  * **等高线地图视角**：按住 `Alt` 呼出光标点击左上角小地图、或按 `M` 进入，战场**自动暂停**，等高线图铺满全屏并以指挥官为中心，滚轮以鼠标为中心缩放（1x~5x，放大后自动换 3072px 细节图），左键/中键拖拽平移，左键点我方番号选中（`Ctrl`/`Shift` 增减）/点敌方番号集火/点空地机动，顶部「返回战场」按钮（或 `M` / `ESC`）恢复原时间流速；
  * **HUD 已精简**：无右上按钮与计时文本、无底部提示黑条，左下敌我兵力为白底墨字卡片（我方蓝条/敌方红条）；
  * **子弹命中代理**：编队拣选盒 `PickBox` 只挡 `Visibility` 不挡弹道；骨骼士兵各挂 `WorldDynamic` 胶囊命中代理（r36/h92），子弹只命中可见士兵，人缝与阵外正常穿过；
  * **命令反馈**：机动命令画出沿寻路路径的虚线、终点靶标圈与到达朝向箭头，集火命令画指向目标的箭头线与红色准星圈，战场第一人称与地图视角均有对应投影；
  * 自检：`ERMapSelfTest` 控制台命令（或 `legion_shot.ps1` 末尾自动执行）模拟缩放/点番号/集火/机动/返回并在日志输出 PASS/FAIL。
* **林地 (`LegionForest.*`)**：种子化散布 `Fishermans_Cabin` 的 `SM_Fir_Tree_01~08`（Nanite、无碰撞），噪声成林、避开陡坡与部署带；树冠写入寻路网格——密林格封锁寻路、疏林格给远程掩体（齐射 ×0.88 / ×0.75）；种子取合同 `TerrainSeed`，`-ForestSeed=N` 覆盖、`-NoForest` 关闭，种完自动校验三条车道仍连通。
* **PBR 受光地形材质 (`Scripts/legion_make_terrain_material.py` → `M_LegionTopo`)**：
  * 切换为 `MSM_DEFAULT_LIT` 接收低角度主光立体明暗与士兵投射阴影，并使用 `fwidth()` 屏幕导数绘制恒定像素宽度的抗锯齿等高线与 25m 参考网格。

---

## 🌌 早期原型定位与功能（保留参考）

`Ebbing Rainbow` 致力于打破传统 2D 卡牌构筑缺乏空间立体感、以及传统战棋地图套路固化的痛点，融合了策略卡牌的解题快感与 3D 战术战棋的空间博弈：

1. **3D WFC 算法程序化地形 (Wave Function Collapse)**
   - 告别千篇一律的固定棋盘。每场战斗前，底层 WFC 算法根据体素邻接规则进行实时空间坍缩，动态生成包含掩体、高低落差与战术隘口的立体随机战场。
   - 当前默认的 **Blackshard 大厅**：76×76 m 基座，中心 24–28 m 见方的双层挑空中庭，外圈一圈 +4 m 环形回廊与方柱柱廊，十字天桥跨过中庭，四向各一部 8 m 宽楼梯把大厅接上回廊，最外圈是 3–4 个 24 m 级、转角削成 45° 阶梯斜面的大房间——宏观体块由建筑语法决定，具体墙/门/柱再由 socket 精确填成 kit 瓦片。
2. **种子可复现的 DAG 航线图 (Route Map)**
   - 一局远征由 10 层、约 25 个节点的有向无环图构成：起点单节点、终点必为战斗，中间每层 2–4 个节点，任意一条通路至少 5 场战斗；
   - 路线由随机种子决定并显示在界面上，**同一个种子必然生成同一张图**，方便复盘、分享与问题定位。
3. **卡牌驱动的 4v4 战术回合博弈**
   - 融合《杀戮尖塔》式的抽牌/弃牌堆机制与行动点水晶（Action Points）；
   - 结合顶部动态流转的**先攻队列 (Initiative Queue)**，让玩家在卡牌费用算计、走位拉扯与行动顺位拦截中寻找最佳破局点。
4. **高沉浸感 3D 战斗反馈**
   - 摒弃纸片人对撞：基于 UE5 Manny 骨骼网格体实现实时的移动跑位与砍杀打击动画；
   - 击杀单位时无缝触发**物理布娃娃 (Ragdoll)** 动力学击飞，配合弧线弹跳的 3D 悬浮伤害跳字。
5. **明日方舟风格的高科技宇宙大厅**
   - 采用高对比度哑光黑曜石切角界面（`WBP_HallHUD`，UMG 蓝图 + C++ `UER*` 基类），左侧展现浩瀚的行星与轨道空间站，右侧集成战术终端、干员编队、实时走字时钟与一键全屏观景（Zen Mode）。

---

## 🏛️ 三关卡核心架构

```
                     ┌──────────────────────────────┐
                     │   空间站大厅 (Lvl_Hall)      │
                     │  - 3D 悬浮空间站 & 行星视景  │
                     │  - 明日方舟风格高科技战术 UI │
                     │  - 编队 / 观景 / 局外整备    │
                     └──────────────┬───────────────┘
                                   │
                          点击 [ 航线管理 ]
                                   │
                                   ▼
                     ┌──────────────────────────────┐
                     │   航线图 (Lvl_Route)         │
                     │  - 10 层 DAG, 种子可复现     │
                     │  - 安全 / 战斗 两类节点      │
                     │  - 队伍完整度跨战斗累积      │
                     └──────────────┬───────────────┘
                                   │
                          点击 [ 战斗 ] 节点
                                   │
                                   ▼
                     ┌──────────────────────────────┐
                     │   战术战场 (Lvl_ConcreteArena)       │
                     │  - 3D WFC 算法随机生成地图   │
                     │  - 4v4 特勤小队先攻博弈      │
                     │  - 手牌构筑 + AP 水晶消耗    │
                     │  - 真实跑动打击 + 布娃娃物理 │
                     └───────┬──────────────┬───────┘
                       胜利 │              │ 失败
                            ▼              ▼
              回到 Lvl_Route 继续选路   回到 Lvl_Hall（本局结束）
```

### 1. 空间站大厅 (`Lvl_Hall`)
* **主要职责**：玩家的大本营、战术整备中心与远征入口。
* **主要功能**：
  * **3D 视界**：支持自由机位漫游与摄像机视差悬浮微倾斜（Parallax）；
  * **战术终端 (Terminal)**：左上角黑曜石 45° 切角主标题碑（配左侧虹彩光谱条与《控制：共振》巨细对比排印），底部居中提供 **`[ 01 人员管理 ]`**（黑曜石）与 **`[ 02 航线管理 ]`**（高反差反白石膏主按钮），后者是进入航线图的唯一入口（没有进行中的远征时会先自动开新局，有则接着走）；
  * **人员管理 (Personnel)**：全屏干员档案终端 —— 顶部 3px 通栏虹彩窄条 + 左侧 2K 无损透明人物原画顶天立地铺满左半屏 + 左下横向 4 干员反白切页坞（支持 `1-4` / `A/D` / 方向键与 `Esc`）+ 右侧 92pt 巨型粗体姓名、148pt 背景代号水印、基本属性、四学派抗性与战术技能；远征进行中时完整度显示当前值。立绘暂统一用根目录 `IMG.PNG` 占位（无损 `TC_EDITOR_ICON` 导入为 `/Game/UI/Textures/T_UI_HeroPortrait_Placeholder`），专属立绘到位后在 `FHallHeroProfile::Portrait` 逐个替换；
  * **特勤编队 (Squads)**：展开 4 人先遣特勤小队属性面板（先锋、斥候、术师、秘术）；
  * **全屏观景模式 (Zen Mode)**：点击左下角 👁 或按 `Esc` 隐藏所有 UI，将大厅转化为宁静的宇宙沉浸观景台。

### 2. 航线图 (`Lvl_Route`)
* **主要职责**：Roguelike 局内的路线选择层，把「一局」拆成一次 10 层的远征。
* **主要功能**：
  * **种子化 DAG 生成**：整张图由 `FRouteGraphGenerator` 从种子一次性生成，右上角常驻显示当前种子；同种子必然同图（节点位置、类型、连线全部一致）；
  * **结构约束（可验证）**：10 层；第 1 层单起点、第 10 层单终点且必为战斗节点；中间每层 2–4 个节点（7 个候选槽位）；**任意一条起点→终点路径至少 5 场战斗**；
  * **节点类型**：当前只有 `安全`（直接前进，无事发生）与 `战斗`（进入 `Lvl_ConcreteArena`），类型完全由 `GNodeTypeTable` 元数据表驱动；
  * **状态可读**：当前节点（金色光环）/ 可选节点（青色描边 + 呼吸）/ 已走节点（暗金）/ 未解锁节点（压暗但仍可见，保证能规划整条路线）；
  * **连线不交叉**：采用阶梯式单调连边（第 i 个上层只连 slot 区间 `[⌊iM/N⌋, ⌊(i+1)M/N⌋]` 的下层节点），结构上不可能交叉，再随机删边制造分支疏密，生成后无需修补；
  * **横向布局**：层级沿 X 轴从左到右推进、槽位沿 Y 轴分布，整张图按窗口等比缩放铺满可用区域（竖向布局在 16:9 下会被压成屏幕中间一条窄带，故改为横向）；
  * **局内状态跨关卡存活**：队伍完整度、当前节点、已走路径都放在 `URunSubsystem`（GameInstance 级子系统），进出战斗关卡不会丢；战斗只生成还活着的队员，胜利后把剩余完整度写回。

**代码分层（从数据到界面）**：

- **`RunTypes.h`**：`ERouteNodeType` / `FRouteNodeTypeInfo` 元数据表 / `FRouteNode` / `FRouteGraph` / `FRouteGenSettings` / `FRunUnitState`，纯数据结构；
- **`FRouteGraphGenerator`**：生成 + 校验 + 最小战斗数 DP（`Generate` / `Validate` / `ComputeMinBattlesPerPath`），所有可调参数集中在 `FRouteGenSettings`；
- **`URunSubsystem`**：整局状态（种子 / 图 / 队伍 / 当前节点 / 战斗场数）与选路接口；
- **`ARouteGameMode` / `ARouteHUD` / `ARoutePlayerController` / `ARouteCameraPawn`**：关卡运行时，支持 `?seed=` 固定种子，`?game=` 由大厅传入；
- **`UERRouteHUD` / `UERRouteNode` / `UERRouteNodeSigil` / `UERRouteEdgeLayer` / `UERRouteLayerRuler`**：UMG 界面层（`WBP_RouteHUD` 蓝图可手调），矢量印记与贝塞尔连线由 Slate leaf 自绘，几何工具共用 `Run/UI/SRouteMapVisuals.h` 的 `RouteVis` 命名空间。

**扩展一个新节点类型**：在 `ERouteNodeType` 加一个枚举值，在 `GNodeTypeTable` 加一行（显示名 / 字形 / 颜色 / 是否战斗），生成器与 UI 都不需要改，只需在 `ARouteHUD::HandleNodeClicked` 里补上该类型的跳转或结算。

### 3. 战术战场 (`Lvl_ConcreteArena`)
* **主要职责**：Roguelike 关卡战斗核心循环。
* **主要功能**：
  * **WFC 地形生成器 (`ABattleArena`)**：开局毫秒级自动构建 3D 战术地图；从航线图进入时会用「节点遭遇种子」驱动，保证同一局同一节点的地形一致；默认宏观布局是 Blackshard 大厅（挑空中庭 + 环形回廊 + 天桥 + 四向楼梯 + 大房间），楼梯与 +4 m 回廊已烘焙进 navmesh，AI 与玩家都能上下；
  * **先攻条 (Initiative Bar)**：顶部芯片式流转显示下一位行动的角色；
  * **战术手牌区**：动态浮动的卡牌（移动卡、攻击卡、技能卡），具备悬停浮起微动效与费用不足灰显判断；
  * **战术摄像机两态 (`ETacticalCameraMode`)**：**自由态**下 WASD / 屏幕边缘 / 中键拖拽自由取景，**聚焦态**下镜头每帧贴着当前行动棋子（敌人走位时同样跟随），跟随目标阵亡时就地转为自由态、不会突然飞回全景锚点；
  * **战斗结算面板**：战斗结束后不再自动跳转，而是弹出玻璃质感结算窗（回合数 / 存活队员 / 队伍完整度 / 预留的得失行），由玩家点击按钮决定返回航线图还是空间站大厅；
  * **调试作弊入口**：右上角 `DEBUG · 秒杀敌人`（仅非 Shipping 构建）一键清空敌方，走与正常战死完全相同的结算链路；
  * **战况日志**：实时通知与战报飘屏播报。

### 4. WFC 三参数生成架构（代码层）

上层只依赖一个稳定入口：

```cpp
FWFCGenerationRequest Request;
Request.Style = EWFCBuildingStyle::Style0; // 进度 -> 风格
Request.Scale = FIntVector(14, 14, 1);     // 建筑规模（格数）
Request.Seed  = EncounterSeed;             // 随机种子
ArenaHost->Generate(Request);
```

- **`AWFC3DArenaHost`**：三参数容器。查 `UWFC3DStyleTable`，创建/复用/销毁具体生成器，不关心任何拓扑。
- **`AWFC3DArenaGeneratorBase`**：通用 WFC 管线（编译模块 → 初始化求解器 → 施加约束 → 求解 → HISM 渲染）。
- **`FWFCArchGrammar`**：宏观建筑语法层。先按种子排出 `FArchPlan`（房间/大厅/中庭/回廊/天桥/楼梯/柱位），再把每个格子的边界解析成 kit 瓦片（`ResolveWallTile` 带 d→p→o、n/h→w 的降级），最后交给生成器实例化并单独铺碰撞盒；`ER.ArchLayout` 就是在这一层切换布局。
- **`WFC3DArenaGenerator_*`**：每种风格一个独立文件（Style0 / ThreeStory / MegaAtrium / SpiralGarden / Dungeon），各自负责模块集与初始约束。
- **`WFC3DSolver` / `WFC3DModuleLibrary` / `WFC3DTypes`**：与风格无关的通用算法、内容编译与数据类型。

新增一种风格：新建 `WFC3DArenaGenerator_XXX.h/.cpp`，在 StyleTable 资产里加一条 `Style -> GeneratorClass` 记录即可，Host、Base、Solver、战斗层都不需要改。

#### 材质与纹理：Futuristic 纹理集

生成的瓦片（`SM_wall_* / SM_floor_* / SM_ceil_* / SM_prop_*`）的材质槽直接指向 `MI_WFC_*`，生成器只是把 SM 塞进 HISM、不覆写材质——所以**换皮只改 MI**，WFC 算法、空间语法、碰撞与 navmesh 都不用动。

- **纹理源**：`Content/Futuristic` 下的 PNG 按 `<名称>_BC / _N / _ORM` 成组导入到 `/Game/Futuristic`（1024²）：
  - `_BC` 基础色（sRGB 开、默认压缩）；`_N` 法线（sRGB 关、`TC_Normalmap`）；`_ORM` 打包遮罩（**R=AO、G=Roughness、B=Metallic**，sRGB 关、`TC_Masks`）。
- **母材质 `M_WFC_TintedPBR`**（唯一被改动的材质）：
  - `BaseColor = BaseColorTex × Tint`；
  - `Normal = NormalTex`；
  - `AO / Roughness / Metallic = ORMTex.R / .G / .B`；
  - 原先直接写死的 `ceiling_mat_Normal / _Roughness / _Metallic` 采样节点已删除，母材质不再引用 Fab 贴图。
- **实例映射**：

| 材质实例 | 纹理组 | 主要用途 |
| :--- | :--- | :--- |
| `MI_WFC_Wall_A` | `T_Wall_TechRecess` | 主墙体 |
| `MI_WFC_Wall_B` | `T_Wall_PortalTechInset` | 门框 / 拱门 |
| `MI_WFC_Wall_C` | `T_Wall_DarkStructuralMetal` | 结构墙、方柱 |
| `MI_WFC_Wall_D` | `T_Wall_WarmSandstone` | 暖色房间墙 |
| `MI_WFC_Dark` | `T_Wall_DarkStructuralMetal`（Tint 0.55） | 踢脚、压条、栏杆 |
| `MI_WFC_Floor_A` | `T_Floor_LargeSlab` | 大厅 / 房间地面 |
| `MI_WFC_Floor_B` | `T_Floor_DeckPlating` | 回廊 / 平台 |
| `MI_WFC_Floor_C` | `T_Floor_TreadGrating` | 格栅、楼梯踏板 |
| `MI_WFC_Ceil` | `T_Ceiling_Panel` | 天花板 |

- **发光条**：`MI_WFC_Emit_A / C / W` 走 `M_WFC_Emissive`（Unlit，`EmissiveColor × EmissiveStrength`），是纯色光带，不参与贴图替换。
- **换纹理后重导 / 重绑**（编辑器需先关闭）：

```bat
"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" D:\EbbingRainbow\EbbingRainbow.uproject ^
  -ExecutePythonScript="D:\EbbingRainbow\Scripts\wfc_import_futuristic_textures.py" ^
  -unattended -nosplash -nullrhi -stdout

"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" D:\EbbingRainbow\EbbingRainbow.uproject ^
  -ExecutePythonScript="D:\EbbingRainbow\Scripts\wfc_apply_futuristic_materials.py" ^
  -unattended -nosplash -nullrhi -stdout
```

> 注意：`Content/WFCKit/Materials` 下的 `.uasset` 如果被标记为只读，保存会静默失败；`wfc_apply_futuristic_materials.py` 会先清掉只读位再保存，两个脚本都是幂等的，可以反复跑。

---

## 🎨 UI 美学与动效规范（Arknights × Control: Resonant）

《虹不我与》全场景 UI 采用 **明日方舟式机能未来主义平面设计** 融合 **《控制：共振》式粗细巨幅排印对比**，整体气质服务于 **静谧、庄严、有神性** 的视觉目标，并以纯 C++ Slate GPU 矢量绘制 + 零分配物理弹簧动效追求性能与平面精度的上限。

### 0. 两条铁律：「白底墨字彩条」统一样式 + 禁止中英文并存

**① 全工程所有按钮与栏目统一为「白底墨字彩条」**（源自大厅「银河委托」主按钮）：反白石膏底 + 极墨黑粗体字 + 左侧 3–4px 《虹不我与》光谱条（或阵营/状态单色点睛条）+ 45° 切角 + 四角对准刻度；悬停提亮并划过扫光，按压弹簧回弹，禁用态整体褪为灰白，选中/紧急态墨框加粗。
- 按钮：一律使用 `UERActionButton`（`UERActionButton::Make(Owner, 文案, 字号)`），大厅入口 `UERHallButton`（已不再提供黑底次按钮）；Canvas 绘制的按钮用 `ALegionHUD::DrawBonePlate`；
- 栏目（章节标题条、列表卡、兵团牌、番号牌、编队切换条）：UMG 用 `ERUI::MakeBoneCard` / `ERUI::MakeSectionBar`，配色一律经 `ERUI::StyleBoneFrame` 与 `ERUI::InkText`，**不得再手写暗色底板**；
- 骨白底上的状态墨色须加深（危险 `(0.62, 0.04, 0.02)`、琥珀 `(0.58, 0.31, 0.0)`、冷蓝 `(0.02, 0.30, 0.62)`），否则对比不足。

**② 禁止中英文同时出现**：同一控件（按钮、栏目、标题、副标、提示、番号牌）内**只允许中文**，不得再配英文副标、英文遥测码或「中文 // ENGLISH」式双语；按钮去掉英文后，中文字号相应加大（按钮主字默认 `22 pt`，主入口 `32 pt`）。
- 兵种/势力代号一律用中文简称（`ERGalacticMeta::GetClassCode` → 重盾/长戟/刃兵/射手），呼号用中文序数（`虹膜一号 // 铁砧盾卫`、`敌军二号`）；货币单位写「信用点」；
- 仅允许保留：阿拉伯数字、键盘键位标识（`M`、`Alt`、`Shift`、`WASD` 等）与第三方产品专名（如 `DEEPSEEK`）；
- 因此下文「英文微标」「英文遥测」等字阶描述仅适用于历史原型，**新增界面一律改用中文微标**。

### 1. 色彩铁律：黑 — 白 — 点睛彩（`DA_ERUITheme`）

严格控制画面色相面积：**90%+ 面积为哑光黑曜石、纯正石膏白与中性灰阶，高饱和彩色仅作为焦点标尺、虹彩窄条、阵营/学派色点与危险态点睛**；拒绝廉价的均匀霓虹发光与软萌圆角渐变。

| 语义角色 | `DA_ERUITheme` 属性 | 线性空间参考值 (Linear RGBA) | 使用场景 |
| :--- | :--- | :--- | :--- |
| **哑光黑曜石底板** | `InkPanel` / `ObsidianElevated` | `(0.008, 0.009, 0.012, 0.92)` / `(0.018, 0.020, 0.026, 0.94)` | 切角面板、主碑、手牌、铭牌、AI 终端底板 |
| **极深纯墨 / 全屏底幕** | `PureInk` / `InkScrim` | `(0.006, 0.007, 0.009, 1.00)` / `(0.004, 0.005, 0.007, 1.00)` | 反白按钮文字、全屏人员档案与结算底幕 |
| **石膏白四阶层次** | `Bone` / `Secondary` / `Tertiary` / `Ghost` | `(0.92, 0.92, 0.90)` @ `1.00 / 0.62 / 0.36 / 0.16` | 主标题、反白主 CTA、正文、微标、1px 发丝线 |
| **折射暖金（主点睛）** | `Accent` | `(0.96, 0.56, 0.04, 1.00)` | 当前焦点、分割线锚块、状态高亮 |
| **冷青 / 控制朱红** | `AllyMark` / `EnemyMark` (`DangerColor`) | `(0.05, 0.56, 0.94)` / `(0.84, 0.05, 0.03)` | 我方/敌方 3px 阵营色条、扣血尾迹、中止/警报 |
| **三段虹彩窄光谱** | `PrismStart` → `PrismMid` → `PrismEnd` | 冷青 → 折射暖金 → 控制朱红 | 3–4px 极细主碑/终端/全屏顶边虹彩标线（呼应片名《虹不我与》） |

### 2. 双字体排印与《控制：共振》巨细对比

通过复合字体资产 `/Game/UI/Fonts/F_ERUI_Composite` 自动分流，**禁止在蓝图中裸绑单一 `UFontFace`**：
- **英文与全部数字**：统一使用 **Linux Biolinum Bold**（`FF_LinBiolinum_Bold`，源文件 `Content/UI/Fonts/Source/LinBiolinum_RB.ttf`），赋予古典人文碑铭与精密仪器并存的锐利气质；
- **中文正文与标题**：统一使用 **Noto Sans SC** 双字重（`FF_NotoSC_Regular` / `FF_NotoSC_Bold`）；
- **夸张字阶对比规范**：在所有文字主导的场面，将 **巨幅粗体主标题/关键数值** 与 **较小的中文粗体微标** 成组配对（英文微标已按 §0 禁用，下表英文示例为历史写法）：

| 字阶 | 典型字号 | 字重与字距 (Tracking) | 典型落点 |
| :--- | :--- | :--- | :--- |
| **Micro 微标** | `8–9 pt` | Regular，`+200 ~ +360` | 英文遥测码（`OPERATOR DOSSIER // SQUAD MANIFEST`、`PRTS // NEURAL TACTICAL LINK`）、属性小标、四角坐标 |
| **Caption / Body** | `10–13 pt` | Regular / Bold，`+20 ~ +160` | 技能说明、战报日志流、图例、按钮副标 |
| **Title / Metric** | `18–32 pt` | Bold，`+10 ~ +80` | 面板标题、人员档案属性数字（`30 pt`）、按钮主字与编号 |
| **Display 碑文** | `38–54 pt` | Bold，`+40 ~ +80` | 大厅 `虹不我与`（`48 pt`）、航线图 `航线图`（`42 pt`）与 `01 / 10`（`38 pt`）、战斗 `HP` 巨字（`52 pt`） |
| **Hero / Watermark** | `92–148 pt` | Bold，`+20 ~ +120` | 人员档案干员姓名（`92 pt`）、战斗结算 `胜 利 / 败 北`（`96 pt`）、人员背景代号水印（`148 pt` @ 4.5% 透明度） |

### 3. 纯 C++ Slate GPU 矢量图元库（`Source/EbbingRainbow/UI/Primitives/ERTechPrimitives.*`）

所有硬朗几何与机能装饰均由 `SLeafWidget::OnPaint` 直接提交 GPU 顶点，**零贴图拉伸模糊、零额外材质开销**，并在 UMG Palette `"ER UI|Tech"` 中暴露为可拖拽控件：
- **`UERTechFrame`**：支持四角独立 45° 切角（`FSlateDrawElement::MakeCustomVerts` 三角扇填充）、1px 锐利轮廓、顶部微高光发丝线、四角精密 L 形对准刻度（自动避让切角处改为 45° 加强斜杠）、单色或三段虹彩边条，以及悬停/入场斜向扫光；
- **`UERTechDivider`**：左侧实体锚块 + 1px 发丝延伸线 + 末端微型菱形，带 `0→1` 动态展开进度；
- **`UERPrismBar`**：3 段 6 顶点 GPU 线性光谱渐变条（水平/垂直通用）；
- **`UERCornerReticle`**：全屏四角精密光学刻度标尺与追踪英文编解码。

> **工程铁律**：`FSlateDrawElement::MakeCustomVerts` **禁止**传入无底层纹理的 `FCoreStyle::WhiteBrush`（`GetResourceHandle` 会返回无效句柄并静默跳过渲染），统一经 `ERUI::GetWhiteTexture()`（进程级纯白 `UTexture2D`）获取有效资源句柄。

### 4. 纯 C++ 零分配 UI 动效引擎（`Source/EbbingRainbow/UI/Motion/ERUIMotion.h`）

不依赖第三方插件，全套动效以零 `UObject`、零堆分配的值类型结构实现，确保高帧率与 UE 5.8 原生稳定：
- **非线性缓动 (`ERMotion::Ease`)**：`ExpoOut`（快出慢落，机能面板/入场主曲线）、`QuintOut`、`CubicOut`、`QuartInOut`、`CrispSettle`、`SolemnPulse`（克制庄严呼吸）；
- **子步进二阶阻尼弹簧 (`FERFloatSpring` / `FERVec2Spring`)**：120Hz 内部子步进防帧率抖动，内置 `Snappy`（按键反馈）、`Tactile`（卡牌悬停上浮 `-14px` & `1.06x` 缩放）、`Monumental`（庄严落位）、`Silky`（血条缓冲与扣血红尾迹）四套调校预设；
- **错峰序贯入场 (`FERRevealTrack`)**：按 `(Delay, Duration)` 编排顶栏、主体、按钮坞与分割线的建筑式逐级显影；
- **遥测译码 (`FERCipherReveal`)**：英文大写编号/代号专用，从左向右高速收敛并在 90% 进度处全部锁定（杜绝尾字停留乱码），收敛后零计算开销。

### 5. 四大场景界面布局规范

1. **空间站大厅 (`WBP_HallHUD` / `WBP_HallButton`)**：左上黑曜石切角 `虹不我与` 碑体 + 左侧 4px 虹彩条；左部标题下方纵向四枚同款「白底墨字彩条」按钮 `01 兵团整备` / `02 银河委托` / `03 银河局势` / `04 设置`（无英文副标，主字 32 pt，左下锚定）；四角光学标尺，保留大片建筑留白。
2. **全屏人员档案 (`WBP_PersonnelPanel` / `WBP_PersonnelMemberRow` / `WBP_PersonnelStatCell`)**：100% 全屏黑曜石底幕 + 顶部通栏 3px 虹彩窄条；**左半屏（0%–56%）全高立绘舞台**采用 `TC_EDITOR_ICON` 无压缩 2K 透明底原画（`IMG.PNG`），通过微负边距吃掉透明留白使人物与机甲顶天立地铺满左半屏且与背景零接缝；左下悬浮横向 4 干员切页坞（选中项反白高对比）；右半屏为 `92 pt` 姓名 + `148 pt` 淡水印 + `30 pt` 属性数字 + 四学派抗性色标与技能芯片。
3. **航线图 (`WBP_RouteHUD` / `WBP_RouteNode` / `WBP_RouteLegendItem`)**：严格执行航线图铁律——**全场仅玩家当前所在节点发光**（大范围辉光交由 3D `M_Route_Halo` 与 Slate 几何像素对齐），其余节点与连线一律不发光，连线只用细实线，不放零星粒子；左上 `42 pt` 粗体 `航线图` 配虹彩竖条，右上 `38 pt` Linux Biolinum 层级计数 `01 / 10`，底部建筑刻度标尺。
4. **战斗 HUD 与 AI 战术终端 (`WBP_BattleHUD` / `UERAICommandPanel`)**：所有前景控件采用高对比哑光黑曜石切角底板，确保在明亮混凝土/自然岛战场上清晰锐利；左下 `HeroPlate` 呈现 `52 pt` 巨型 HP + 双层弹簧扣血条 + `18×5px` 细长 AP 格；底部 `98×132px` 切角战术手牌配反白费用章与底部学派色条；右上可折叠 **PRTS 风格 AI 战术终端**（`508px` 宽黑曜石切角框 + 左侧 3.5px 虹彩光谱条 + `TELEMETRY // UNIT INDEX` 单位索引 + `CMD/TOOL/RET/PRTS/ERR/SYS` 结构化标签日志 + 反白石膏主 `执行` 按钮）。

### 6. UI 资产自动化重建脚本

全套字体、主题资产、2K 原画与 18 个 WBP 蓝图均由幂等脚本 [Scripts/upgrade_ui_arknights.py](Scripts/upgrade_ui_arknights.py) 一键生成与校验（自动清除 `Content/UI/` 只读位并对 `EAL.save_asset` 返回值做强断言，防止静默保存失败）：

```powershell
# 确保已编译最新 C++ 且 UnrealEditor 处于运行状态（SteerAgents MCP 就绪）
$env:PYTHONPATH = "D:\SteerAgents\client\src"
& "D:\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" Scripts/upgrade_ui_arknights.py
```

---

## 🛠️ 技术选型与技术栈

| 模块 | 技术方案 | 优势说明 |
| :--- | :--- | :--- |
| **游戏引擎** | Unreal Engine 5.8 | 现代渲染管线、Lumen 动态全局光照、Substrate 材质系统 |
| **核心算法** | 3D Wave Function Collapse (C++) | 原生 C++ 实现的高性能多层体素邻接匹配与随机坍缩 |
| **关卡结构** | 自研 DAG 路线生成器 (C++) | 种子可复现、结构约束可校验、节点类型可扩展 |
| **UI 系统** | **UMG 蓝图套件 + C++ Slate 图元** | `WBP_*` 可视化蓝图承载布局，`UER*` C++ 控件内嵌 Slate leaf 矢量绘制与零分配弹簧动效 |
| **物理与动画** | Chaos Physics + Motion Warping | 真实死亡布娃娃物理模拟、平滑移动轨迹与动画打击同步 |
| **寻路与 AI** | Navigation System (RecastNavMesh) | 战术网格多线程寻路，动态障碍物规避 |

---

## 🧪 实验性模块：AI 战术指令代理（毕业设计创新点）

> **定位声明**：本模块**仅为毕业设计阶段的实验性技术探索与创新点展示，并非《虹不我与》正式版的核心玩法流程**。

* **高度独立与一键剥离**：
  * 核心代码全部隔离在 `Source/EbbingRainbow/Experimental/AITacticalCommand/`（`UBattleAICommandService`、`UERAICommandPanel`），不改动 `WBP_BattleHUD` 资产与核心战斗结算规则；
  * 通过 `Source/EbbingRainbow/EbbingRainbow.Build.cs` 中的 `bEnableExperimentalAI`（控制 `ER_WITH_AI_COMMAND` 编译宏及 `HTTP`/`Json` 模块依赖）统一管理。**正式发布时只需将其设为 `false`**（或删除 `Experimental/AITacticalCommand/` 目录），即可彻底剥离该功能。
* **实现机制**：
  * 战斗 HUD 右上角挂载可折叠的 `AI 连接` 面板，玩家可输入自然语言战术指令（如“让第一个棋子去攻击序号为1的敌人”）；
  * 内置战术提示词与多轮对话上下文管理，将实时战场状态（单位编号 `P1..Pn` / `E1..En`、HP/AP、技能表、3D 视线/掩体/高低差）连同 Function Calling 工具表发送至外部大模型 API（兼容 OpenAI `/v1/chat/completions` 与 Anthropic `/v1/messages` 格式），并异步驱动棋子寻路移动、技能施放、镜头聚焦与回合推进，同时在主 HUD 战报条与面板中输出中文战术播报；
  * API 配置与 Key 仅保存在本地未跟踪目录 `Saved/ER_AIConfig.json`，不会进入 Git 仓库。

---

## 🎮 操作指南

### 军团大地图战场 (`Lvl_Legion`)
**战场视角（第一人称，默认）**
* **移动 / 视角**：`WASD` 移动、鼠标转向、`Shift` 疾跑、`空格` 跳跃；按住 `Alt` 临时呼出光标（点小地图、兵团牌、按钮）；
* **左键**：纯射击（手枪），命中敌团士兵结算伤害；
* **右键（全权指挥）**：点我方兵团选中（再点已唯一选中的兵团取消；`Ctrl` 增减选）、点敌方兵团/番号集火、点地面机动（按住拖动 >350cm 可拉出阵型驻守朝向）；`1 ~ 8` 快选；
* **军团指令**：`Z` 主动协议、`X` 停驻、`G` 全军突击、`F` 选中兵团（无选中则全军）跟随/取消跟随、`R` 重新部署、`ESC` 取消选中/关地图；

**等高线地图视角**（按住 `Alt` 点小地图或按 `M`；战场自动暂停）
* **缩放 / 平移**：滚轮以鼠标为中心缩放，左键拖拽或中键拖拽平移；
* **指挥**：左键点我方番号选中（`Ctrl`/`Shift` 增减），再点敌方番号集火，点空地（或右键）机动；
* **退出**：顶部「返回战场」按钮 / `M` / `ESC`；
* **自动化截图**：`powershell -ExecutionPolicy Bypass -File Scripts/legion_shot.ps1`（起 `-game` 进 `Lvl_Legion` 截取开局第一人称、交战俯拍、回到第一人称、等高线地图视角、命令反馈五张图，并跑地图交互自检）。

### 大厅关卡 (`Lvl_Hall`)
* **鼠标左键 / `Esc`**：第一人称漫游下按 `Esc` 释放光标，点击左部 **`01 兵团整备`** / **`02 银河委托`** / **`03 银河局势`** / **`04 设置`** 进入终端面板对应页签，`返回大厅` 退回；
* **命令行截图参数**：`-HallShot`（大厅主界面）、`-GalaShot`（银河委托终端）。
* **编辑器机位调整**：在大纲中选中 `HallCameraPawn`，按 **`Ctrl + Shift + P`**（驾驶视角）漫游调整至满意角度，或按 **`Ctrl + Shift + F`**（对齐到视口）。

### 航线图 (`Lvl_Route`)
* **鼠标左键点击可选节点**：`安全` 节点直接前进（不切关卡），`战斗` 节点进入 `Lvl_ConcreteArena`；
* **右上角信息条**：常驻显示本局种子与进度（第 X / 10 层 · 战斗 N 场）；
* **固定种子**：以 `Lvl_Route?seed=12345` 启动可复现同一张图，便于复盘与排查；
* **调试命令**（控制台）：`RouteDump` 打印整张图、`RouteSelect <节点Id>` 走一次与鼠标点击完全相同的逻辑、`ER.AutoWin 1` 让战斗 4 秒后自动判胜以快速验证「战斗→航线图」回环。

### 战斗关卡 (`Lvl_ConcreteArena`)
* **鼠标左键**：选中手牌、点击地图格子位移、点击敌方单位施法/攻击；
* **快捷键 `1 ~ 4`**：快速选定手牌槽位的对应卡牌；
* **点击右下角金色按钮 `[ 结束回合 ]`**：将先攻权移交给下一位角色；
* **战术镜头**：`WASD` / 屏幕边缘推屏 / 中键拖拽进入自由取景（同时退出聚焦），`Q` `E` 旋转，`F` 聚焦当前行动单位，滚轮缩放；聚焦态下镜头会自动跟随该棋子移动；
* **右上角 `[ DEBUG · 秒杀敌人 ]`**：作弊按钮，清空敌方并直接弹出战斗胜利结算（仅非 Shipping 构建可见）；
* **结算面板**：战斗结束后点击面板按钮 —— 航线图战斗胜利时是 `[ 返回航线图 ]`，其余情况是 `[ 返回空间站大厅 ]`；
* **调试命令**（控制台）：`BattleCheatWin`（等同作弊按钮）、`BattleResultReturn`（等同结算返回按钮）、`ER.AutoWin 1` 自动判胜 / `ER.AutoWin 2` 判胜后自动返回、`ER.AutoPlay 1` 我方回合自动结束（让 AI 自己打完整场，便于观察镜头跟随）；
* **地形调试命令**（控制台）：`ER.ArchLayout 0|1|2` 切换宏观布局（0=旧房间图，1=旧十字中庭，2=Blackshard 大厅，默认 2，下一场生效）、`ER.ArchDump 1` 把平面图以 ASCII 打进日志（`#` 房间 `.` 大厅 `~` 回廊 `o` 柱 `=` 天桥 `D` 门）、`ER.AutoShot 1` 就绪后自动切俯瞰相机截图到 `Saved/Screenshots/WindowsEditor/`；三者也可用命令行 `-ArchDump` / `-AutoShot` 在启动时就生效。

---

## 🗺️ 未来规划 (Roadmap)

- [x] **DAG 航线图**：种子化 10 层路线、安全/战斗节点、队伍完整度跨战斗累积。
- [x] **Blackshard 竞技场重构**：宽大厅 + 少而大的削角房间 + 挑空中庭 + 环形回廊 + 天桥 + 四向楼梯，楼梯已接入 navmesh（`Mezzanine path: partial=no`）。
- [ ] **空间与地形交互机制**：引入带有击退 (Knockback) 和牵引 (Pull) 属性的卡牌，实现“撞墙眩晕”与“坠落深渊”。
- [ ] **职业协同与合击卡组**：强化小队连携，设计跨角色状态联动（如易伤标记、元素感电）。
- [ ] **空间站局外基建循环 (Meta-progression)**：完善大厅中的采购中心 (Store)、空间基建 (Base) 与公开招募 (Recruit)，形成完整的局外养成闭环。

### 航线图：下一步建议（当前实现状态）

当前 `Lvl_Route` 已能跑通完整循环：大厅进入 → 图上选路 → 战斗 → 胜利回图（带着残血）/ 失败回大厅结束本局。10 层、约 25 个节点、两类节点、种子可复现，`FRouteGraphGenerator` 的约束已在 1000 个种子上验证通过。

下一步按优先级：

1. **更多节点类型**：精英战 / 商店 / 事件 / 休整 / 宝箱。只需在 `ERouteNodeType` 加枚举、在 `GNodeTypeTable` 加一行，再在 `ARouteHUD::HandleNodeClicked` 补一条分支；
2. **难度曲线**：按层数提升敌人数量与编成，遭遇种子已经按节点派生（`MakeEncounterSeed(RunSeed, NodeId)`），可直接接难度表；
3. **队伍状态上图**：在航线图上显示 4 名队员的剩余完整度，让「走哪条路、要不要绕开战斗」真正成为取舍；
4. **存档 / 继续**：`URunSubsystem` 目前只活在内存里，序列化后即可支持退出重进；
5. **地图表现**：节点继续变多时可加轻微推拉或聚焦当前层，现在整图铺满不滚动。

### WFC 竞技场：当前实现状态

`Lvl_ConcreteArena` 的 WFC 现在有三套宏观布局，用 `ER.ArchLayout` 切换（0=旧房间图，1=旧十字中庭，2=Blackshard 大厅，默认 2）：

**Blackshard 大厅（默认）**

- 76×76 m 基座，由内到外：24–28 m 见方的双层挑空中庭 → 中庭外 1 格宽、+4 m 的环形回廊 → 回廊外侧一圈 4 m 方柱柱廊 → 最外圈 24 m 见方的大厅/房间；
- 中庭正中两条 8–12 m 宽的天桥十字交叉，四向各一部 8 m 宽的纪念性楼梯（`FArchStair::Dir` 支持 ±X/±Y 四个上行方向，模型按方向旋转，碰撞用 20 级台阶盒铺出可走坡道）；
- 3–4 个 24 m 级大房间，朝中庭的转角削成阶梯状 45° 斜面（L 形而不是方块），每条临大厅的边开 1–3 个 2 格宽（8 m）门洞；
- 至多一个象限留作柱林广场；房间内部沿长边每两格立一根方柱（列柱厅）。
- 楼梯落点在回廊**外侧**一格：最后一级台阶顶面 416 cm 与回廊楼板齐平，导航 `partial=no` 可直接从大厅走上 +4 m 回廊（台阶碰撞是 20 级轴对齐盒，每级 20 cm，低于 `AgentMaxStepHeight=35`）。
- 实测（连续 6 个随机 seed）：`rooms=3–4`、`doors=4–9`、`stairs=8`、`atrium=6x6–7x7`、`Upper mezzanine nav: ok`、`Stair mid nav: ok`、`Mezzanine path: partial=no`、`Arena ready attempts=1`。

**调试**

- `ER.ArchLayout <0|1|2>`：切换布局风格（下一场生效）；
- `ER.ArchDump 1`（或命令行 `-ArchDump`）：把平面图以 ASCII 打进日志（`#` 房间、`.` 大厅、`~` 回廊、`o` 柱、`=` 天桥、`D` 门）；
- `ER.AutoShot 1`（或命令行 `-AutoShot`）：竞技场就绪后自动切一台俯瞰相机并截图到 `Saved/Screenshots/WindowsEditor/`，用于离线检查生成结果。

**下一步按优先级**

1. **门洞统一为 2 格宽**：目前短边仍会退化成 1 格门，可把门洞改成“至少 2 格、必要时削掉相邻角”；
2. **把回廊接进战斗逻辑**：给 L1 加射界/高度优势判定，让楼梯与天桥真正成为战术选择；
3. **体块再分层**：大房间局部加 +4 m 平台或夹层，形成三级高差；
4. **验收标准**：`rooms 3–4`、每间 ≥1 个门洞、`Upper mezzanine nav: ok`、`Mezzanine path: partial=no`、`Arena ready` 一次通过，并保留随机 seed 的复现日志。
