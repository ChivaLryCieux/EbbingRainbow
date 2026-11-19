# Ebbing Rainbow (虹不我与)

> **科幻肉鸽回合制 CRPG (Sci-Fi Roguelike Turn-Based CRPG)**  
> 结合 **3D 波函数坍缩算法 (WFC)** 程序化随机地形、**种子可复现的 DAG 航线图** 局内路线选择、**卡牌构筑 (Deckbuilder)** 与行动点 (AP) 策略博弈、以及 **明日方舟风格** 高科技宇宙大厅的战术角色扮演游戏。

---

## 🌌 游戏定位与核心特色

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
   - 采用纯 C++ Slate 编写的高对比度半透明亚克力界面，左侧展现浩瀚的行星与轨道空间站，右侧集成战术终端、干员编队、实时走字时钟与一键全屏观景（Zen Mode）。

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
  * **战术终端 (Terminal)**：清晰展示理智值与战况，底部居中提供 **`[ 人员管理 ]`** 与 **`[ 航线管理 ]`** 两个玻璃按钮，后者是进入航线图的唯一入口（没有进行中的远征时会先自动开新局，有则接着走）；
  * **人员管理 (Personnel)**：玻璃模态面板 —— 左侧四名先遣特勤的成员列表，右侧档案 = **左立绘 / 右名字·学派·基本属性**；名字与学派分开（磐·力学、燧·热学、霆·电学、蚀·化学），显示完整度/行动点/先攻/闪避/暴击/暴伤/命中、四学派抗性与战术技能；远征进行中时完整度显示当前值。立绘暂统一用根目录 `IMG.PNG` 占位（导入为 `/Game/UI/Textures/T_UI_HeroPortrait_Placeholder`），专属立绘到位后在 `FHallHeroProfile::Portrait` 逐个替换；
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
- **`SRouteMapOverlay` / `SRouteEdgeLayer` / `SRouteBackdrop` / `SRouteNodeWidget`**：纯 Slate 界面，负责背景渐变与星点、贝塞尔连线、节点按钮与提示条。

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

## 🛠️ 技术选型与技术栈

| 模块 | 技术方案 | 优势说明 |
| :--- | :--- | :--- |
| **游戏引擎** | Unreal Engine 5.8 | 现代渲染管线、Lumen 动态全局光照、Substrate 材质系统 |
| **核心算法** | 3D Wave Function Collapse (C++) | 原生 C++ 实现的高性能多层体素邻接匹配与随机坍缩 |
| **关卡结构** | 自研 DAG 路线生成器 (C++) | 种子可复现、结构约束可校验、节点类型可扩展 |
| **UI 系统** | **Slate (C++) + UMG 混合架构** | 底层纯 Slate 保证零 GC 负担与高帧率动画，上层 UMG 方便视口挂载 |
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

### 大厅关卡 (`Lvl_Hall`)
* **鼠标左键**：点击 UI 磁贴与底部按钮；
* **点击 `[ 航线管理 ]`**：进入航线图，开始或继续一局远征；
* **键盘 `Esc` / 点击左下角眼睛图标**：切换全屏沉浸观景模式（隐藏/显示 UI）；
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
