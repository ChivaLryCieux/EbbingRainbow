# Ebbing Rainbow (虹不我与)

> **科幻大兵团军事指挥模拟游戏 (Sci-Fi Galactic Military Command Simulation)** · UE 5.8  
> **品类定位：第一人称指挥官视角的即时战术模拟（First-person RTT）× 银河战役经营** —— 肉身在场版的《绝境北地》：弱化建造、强化单兵存在感的「指挥官亲临战场」混合体，叠加银河尺度的雇佣兵战役 Meta。非回合制战棋、非肉鸽——第一人称不是玩法本体，而是指挥视角的载体。  
> 玩家扮演**虹膜基金会 (Iris Foundation)** 研究员，通过虹膜神经接驳技术将战术 AI 接入前线单位，围绕 **「我们面向全银河提供军事指挥服务」** 承接泰拉帝国 / 自由叛军 / 边缘自治领委托，在大尺度起伏地形上指挥多支兵团展开类《绝境北地》式即时战术对抗并改写银河战局。  
> *(仓库同时保留早期 3D WFC 程序化战场、种子化 DAG 航线图与 4v4 回合制卡牌原型代码。)*

---

## 🛸 新策划案核心架构：虹膜基金会银河军事指挥（当前主线）

```
                ┌──────────────────────────────────────────────────────┐
                │   虹膜基金会总署大厅 (Lvl_Hall / WBP_HallHUD)         │
                │   口号:「我们面向全银河提供军事指挥服务」             │
                │   标题菜单: [ 继续游戏 ] [ 开始新游戏 ]              │
                │             [ 读取存档 ] [ 设置 ]                    │
                └───────────────────────┬──────────────────────────────┘
                                        │ 显式载入/创建战役后进入第一人称
                                        ▼
                ┌──────────────────────────────────────────────────────┐
                │   《深岩银河》式第一人称备战大厅                      │
                │   - WASD/鼠标/跳跃/疾跑漫游, 无武器交互              │
                │   - Esc 回标题; 战役状态保留在 GameInstance          │
                │   - 玩家自行放置 HallConsoleActor 作为终端入口        │
                └───────────────────────┬──────────────────────────────┘
                                        │ 走近控制台, 按 E 接入
                                        ▼
     ┌─────────────────────────────────────────────────────────────────┐
     │ 虹膜指挥终端 (UERGalaCommandPanel)                              │
     │ [ 接取委托 ] [ 兵团整备 ] [ 银河局势 ] [ 终端设置 ]             │
     │ - 横向三张合同卡片 / 出击部署                                   │
     │ - 常备兵团补员、扩编与虹膜 AI 同步升级                          │
     │ - 黑灰控制率饼图、6 星区归属与上一战报                          │
     └────────────────────────────────┬────────────────────────────────┘
                                      │ 接取委托并出击
                                      ▼
                    ┌──────────────────────────────────────┐
                    │  大兵团战术战场 (Lvl_Legion)          │
                    │  - PBR 抗锯齿等高线大地形             │
                    │  - 2D 坡度寻路 + 黄金角散阵           │
                    │  - 真实弹道逐兵命中/击杀              │
                    │  - 等高线地图视角 + AI 战术指挥       │
                    └──────────────────┬───────────────────┘
                                       │ 胜利 / 撤离结算
                                       ▼
                         写回 UGalacticCampaignSubsystem
                         (经费 / 战损 / 声望 / 银河控制率)
                                       │
                                       ▼
                         返回 Lvl_Hall 并直接落地第一人称
```

### 1. 宏观战役、《深岩银河》式备战大厅与虹膜终端 (`Campaign/` + `Hall/` + `UI/ERGalaCommandPanel.*`)
* **`UGalacticCampaignSubsystem`**（`GameInstanceSubsystem` 跨关卡持久化）：
  * **三大阵营**：泰拉帝国（`Empire`）、边缘自治领（`Dominion`）、自由叛军（`Rebels`），维护各自银河控制率（归一化和 100%）、基金会声望与 6 个星区归属；
  * **常备兵团编制 (`FManagedLegionData`)**：统一采用旅级中文番号（`第一旅`、`第二旅`、`第三旅`、`第四旅`…）与兵种占位 Logo，记录每支兵团兵种、战损剩余人数、`SyncLevel`（1~5 级虹膜 AI 同步加成）；战后需消耗经费补充伤亡兵员；
  * **动态合同 (`FGalacticContract`)**：每周期生成覆盖三方势力的委托，含敌方多兵种、延迟空降增援波次及地形战术倾向 `ELegionTacticalProfile`（均衡野战 / 高地固守 / 仰攻夺高 / 峡谷伏击 / 开阔机动），刷新加密频道同步推进 `ContractRefreshNonce` 派生新地形种子；卡片同步显示任务地点、战术倾向与酬金；
  * **原生战役存档 (`UGalacticCampaignSaveGame`)**：经费使用「银河币」，战役资金、兵团编制、声望/控制率、当前合同与最近战果在每次成功变更后自动写入 UE 原生 `.sav` 双槽轮换快照；桌面版位于项目 `Saved/SaveGames/ER_GalacticCampaign_{A,B}.sav`。这两个内部轮换槽同时作为标题菜单里的**战役检查点**展示：按世代最新优先排序，可显式回滚；开始新游戏会把上一战役最新档保留在兄弟槽中，选择较旧检查点会写成更高世代的新分支而不删除原最新档。损坏槽位自动回退可用备份并保留原文件；检测到更高版本客户端写出的存档时，继续/新游戏/手动读档都会拒绝覆盖。普通 PIE 为避免污染真实存档使用内存沙盘，不写磁盘。
* **标题菜单 (`ERHallHUD`)**：
  * 启动时只显示主标题碑（口号 **「我们面向全银河提供军事指挥服务」**）与四个入口：`继续游戏`、`开始新游戏`、`读取存档`、`设置`；没有可用存档时「继续游戏」禁用；
  * 「继续游戏」读取最新有效检查点，「开始新游戏」创建新战役分支，「读取存档」列出 A/B 检查点的周期、经费、世代与写入时间；三者成功后都会进入第一人称；
  * 「设置」打开终端设置页的 settings-only 模式，隐藏委托/整备/局势页签，防止从标题菜单绕过空间控制台直接进入游戏流程；
  * 标题模式下使用 `HallCameraPawn` 的观景机位与轻微视差；进入第一人称时相机混合回玩家 pawn，Esc 则从第一人称返回标题菜单。
* **第一人称备战大厅 (`HallGameMode` / `HallPlayerController` / `HallFirstPersonCharacter`)**：
  * 大厅采用《深岩银河》式空间化备战循环：玩家不再从菜单直接点开战役面板，而是在大厅中以第一人称走动，亲自接近终端设备；
  * `AHallFirstPersonCharacter` 复用 Legion 战场的 Enhanced Input、第一人称相机、WASD/鼠标/跳跃/疾跑栈，但清空武器并禁用开火/切枪；光标释放、标题菜单或任意面板打开时移动/视角/跳跃/疾跑都会暂停，关闭后准确恢复；
  * `AHallConsoleActor` 是可由关卡设计师自由放置的终端入口，带交互球体与微弱屏幕光；玩家进入 `InteractionRadius` 并满足视线朝向条件后，底部显示 `E · 接入虹膜指挥终端`，按 `E` 才打开游戏流程面板。`InitialTab`、交互半径、近距离免朝向阈值与提示文案都可在实例上调整；
  * 终端关闭后精确恢复打开前输入状态：第一人称继续漫游回到 game-only 输入，标题菜单打开的设置页则回到 UI 输入；面板透明区域不会误触底层菜单。
* **虹膜指挥终端 (`UERGalaCommandPanel`)**：
  * 终端内部导航为 `接取委托 / 兵团整备 / 银河局势 / 终端设置` 四页，关闭按钮为「断开连接」；
  * **接取委托页**：从左至右横向三等分委托卡片（雇主→敌对→战区 / 简报 / 敌情与酬金 / 出击部署），卡片撑满页高；
  * **银河局势页**：左列用黑灰环形饼图展示三方总体控制率，扇区面积按实际控制率绘制；右列图例保留精确控制率、声望和势力简报，下方显示星区归属与上一战况；
  * **终端设置页**：提供 AI 战术指挥接入配置；战役存档读写失败时可在此重试。画面与音频设置暂未开放。

### 2. 大兵团战场与《绝境北地》式操控 (`Experimental/LegionBattle/`)
* **多兵种克制、旅级番号与占位 Logo (`LegionTypes.h` / `LegionUnit.*` / `GalacticCampaignTypes.h`)**：
  * **当前版本四兵种统一为射手数值**（2026-12 真实弹道改造）：全部使用同一套步枪射程/射速/命中参数进行远程射击对抗，近战差异化参数暂时移除（兵种 Logo/番号标识保留，克制体系待后续围绕真实弹道重建）；
  * 原四兵种定位：重盾卫队 (`SHD`) / 长戟方阵 (`PIK`) / 突击刃兵 (`VGD`) / 精确射手 (`MRK`)；
  * **统一称谓与 Logo 占位**：我方按 `第一旅` / `第二旅` / `第三旅` / `第四旅`… 编号，敌方按 `敌第一旅`…（演武）或 `<阵营简称>第一旅`…（委托）编号；敌我统一复用技能图标作为部队 Logo 占位（我方 `T_Blue_*` / 敌方 `T_Red_*`），贯穿大厅整备、战场标牌、敌军通天旗与等高线地图。*(当前版本暂移除兵团主动技能系统与相关技能 UI，聚焦纯兵种克制与大兵团机动集火。)*
* **自然散落编队与敌我统一动画**：
  * 放弃方格矩阵与刚性 3D 框线，改用**黄金角向日葵采样 + 扰动 + 4 轮最小间距排斥松弛**形成自然散落战团；
  * 旋转时兵团外框不整体甩动，改为每名士兵沿各自原地独立平滑转向 (`S.CurYaw`)；
  * 敌我双方统一采用 `SKM_Manny_Simple` 骨骼网格体、FP 射击模板手枪动画（`MF_Pistol_Idle_ADS` 瞄准站姿 / `MF_Pistol_Jog_Fwd` 低携冲锋 / `MM_Death_*`）与 Ragdoll 物理击飞，右手 `hand_r` 骨骼恒等附加 `SM_Pistol` 手枪网格，通过动态材质 `"Paint Tint"` 区分阵营色。（注：`MM_Pistol_Fire` 等开火动画为 additive 叠加层，单节点实例无法播放，交战视觉由 ADS 瞄准站姿+弹道曳光承担。）
* **士兵表现层技术选型铁律（2026-12 定案：表现力优先，规模服从表现）**：
  * **禁用 HISM/ISM 作为战斗士兵表现**：静态网格实例在原理上无法播放骨骼动画、BlendSpace 与 Ragdoll，产出的是动作僵硬统一的僵尸兵团，已亲眼否决；普通 HISM 能做到的只有逐实例变换与少量材质 custom data，不是"生动士兵"。人数超过承载上限时必须在**策划层限制编制规模**，不得在技术层静默退化为实例化表现；
  * **每兵一个 `USkeletalMeshComponent` 是唯一正式表现路径**：所有优化预算投入骨骼路径本身——屏幕外/远距离动画降频跳帧（URO 思路）、变换批量写入与脏标记、骨骼 LOD 裁剪、关闭多余组件 tick 与物理、远距离隐藏手枪网格；
  * **规模上限由实测帧预算导出，不是设计目标**：以固定种子逐档（50/100/200/300/400 骨骼兵）测量帧耗时得出 60fps 上限，再反推编制表；参照物本身即为证据——《绝境北地》全场仅几十到一百余人、生动感来自动画质量×反馈密度，《骑马与砍杀》用纯骨骼网格+距离 LOD/跳帧支撑数百人，两者均未使用任何实例化替身技术；
  * **ISKM（`UInstancedSkinnedMeshComponent`，UE 5.8 原生）是唯一保留的规模升级研究通道**：逐实例动画 track 支持独立动作/相位/速率/BlendSpace 输入，是唯一能同时满足"大规模+真骨骼变形"的方向；但 5.8 刚落地、无 AnimBP、官方自述稳定性未知，只做**限时独立原型**（验收：逐兵独立相位、阵营染色、挂枪 socket、逐实例命中、死亡动画、CPU/GPU 帧耗时），不通过则永久接受骨骼路径人数上限，绝不迁就；
  * **VAT（顶点动画纹理）明确否决**：近景 artifacts、无动作混合过渡、无精确 socket、无 Ragdoll，与"每兵经得起看"根本冲突；唯一保留用途是永不进入战斗的远景氛围装饰（行军剪影等），属美术点缀而非战斗单位；
  * **玩法策划必须与技术/表现相协调**：编制规模、同屏人数、战役节奏以实测表现力为硬约束——先以 16 人小队"生动度原型"定死质量标准（走跑过渡混合、受击顿挫、死亡多样性、动作相位错开不齐步走），再按帧预算反推最大编制；表现力做不到的规模，宁可不做；
  * **优化验收顺序**：先修骨骼回归 → 写性能标尺场景测上限 → 按上限定编制设计 → 动画生动度原型 → ISKM 限时原型（可选）。
* **轻量化地形寻路与黑白等高线小地图 (`LegionTerrainGrid.*` / `LegionHUD.*`)**：
  * `FLegionTerrainGrid` 开局以当前战区中心 `DeployCenter` 采样地形生成 `2D` 坡度网格（`250 cm` 栅格，局部战区默认 `320m × 320m` / `128×128` 格），Grid A* + 视线拉直自动绕开 `>38°` 陡崖，并提供高低差伤害加成；
  * 运行时由高度场双线性合成黑白等高线图（`256` 小地图 / `1536` 全图 / `3072` 放大细节三档纹理，烘焙多线程并按分辨率放粗线宽；林地区域带斜纹暗影），始终保持正北（`+Y`）朝上；点击小地图或按 `M` 进入**等高线地图视角**（见下「3C」）。
* **3C：FP 模板第一人称指挥官 + 等高线地图双视角 (`FPS/LegionCommanderCharacter.*` / `LegionPlayerController.*`)**：
  * **战场视角（默认）**：玩家是一名第一人称指挥官（UE FP 模板射击栈 `ALegionCommanderCharacter`：手臂+手枪，`Variant_Shooter` 蓝图栈经 `TP_FirstPerson` 重定向挂载；Enhanced Input 驱动 WASD/视角/开火），WASD 移动、鼠标转向、`Shift` 疾跑、`空格` 跳跃；**左键纯射击**（仅地图开着/光标呼出时不打，打中自己人无效）、**右键全权指挥**（点我方兵团选中/再点取消，`Ctrl` 增减选；点地面机动，按住拖出阵型朝向，`F` 令兵团列队跟随）；按住 `Alt` 临时呼出光标点 HUD 或小地图；
  * **敌军通天 Logo 标杆与抬头集火防误触**：敌方不再在头顶贴地显示低空番号牌，而是自地面竖起连续朱红通天标杆直插天空（`6000 cm`），顶端高悬兵种占位 Logo 与番号血条；敌方地面拣选盒 `PickBox` 忽略 `ECC_Visibility`，水平视线右键下达地面机动时不会误触敌人，下达集火需**抬头瞄准空中 Logo 再右键**；
  * **真实弹道模拟 (`LegionGameMode::SimulateBullets`)**：士兵交战不再是即时结算的概率伤害——每名士兵按独立冷却 (`FireTimer` 带相位抖动) 自动瞄准索敌开火，子弹以实体弹道（`5200 cm/s`）逐发飞行，扫描命中逐兵扣血、地形遮挡吞弹，**射不中就不掉血**（《绝境北地》式真实实时模拟）；HUD 以青色（我方）/橙红（敌方）曳光绘制弹迹；
  * **精准逐兵子弹击杀**：每名士兵拥有独立 `HP` 与 `WorldDynamic` 胶囊命中代理（r36/h92），子弹命中时通过 `ALegionUnit::ApplySoldierProjectileHit` 精确匹配被击中的士兵实例（不再在军团内随机或按序号死亡）；击杀瞬间立即禁用该兵命中代理，并沿弹道方向施加布娃娃击飞冲量；
  * **战场 3D 直挂 HUD 与三要素兵团栏 (`LegionHUDWidget` / `ALegionHUD`)**：弃用白底卡片，采用与战终结算一致的《控制：共振》式**无底卡纯白粗体 + 黑色错位立体阴影**排印——左上 `44 pt`「端点星」配虹彩竖条，左侧兵团栏以「`3px` 阵营色竖条（带黑影）+ 部队 Logo + 旅级番号与兵员数量」三要素轻量悬浮（选中态亮起暖金竖条与文字并轻微右移），左下敌我兵力以阵营色竖标 + 纯白错位阴影文字呈现「我方 N 旅 · X 兵 / 敌方 N 旅 · X 兵」，右上「接入 AI」切换钮启用 `SetResonantHudStyle(true)`，战场内我方头顶旗标、敌方高空通天标牌、准星集火提示均通过 `ALegionHUD::DrawResonantText` 渲染为无底盒阴影纯字；
  * **等高线地图视角重构与防碰撞排版**：按住 `Alt` 点左上小地图或按 `M` 进入，战场**自动暂停**并自动隐藏左侧兵团栏与右上角势力 Logo（防止与「返回战场」重叠）；右上角「返回战场」改为虹彩竖条 + 纯白错位阴影文字按钮，下方挂载「接入 AI」入口；地图上敌我番号采用暗曜石半透底板 + 阵营色条 + 大尺寸 Logo（`38px` / 选中 `42px`）+ 纯白阴影文字，并采用三阶段防碰撞求解器（簇状径向扇形预展 + 28 轮 AABB 排斥与 UI 禁区避让 + 零重叠螺旋兜底 + 边缘引线），避免密集交战时标牌重叠；
  * **命令反馈**：机动命令画出沿寻路路径的虚线、终点靶标圈与到达朝向箭头，集火命令画指向目标的箭头线与红色准星圈，战场第一人称与地图视角均有对应投影；
  * **自检与离线截图**：控制台 `ERMapSelfTest`（地图缩放/选团/集火/机动/返回）、`ERSkyBannerSelfTest`（平视防误触/抬头通天 Logo 拣选集火），命令行 `-SkyShot` / `-MapShot` 及 `Scripts/legion_shot.ps1` 支持离线自动验收。
* **林地 (`LegionForest.*`)**：种子化散布 `Fishermans_Cabin` 的 `SM_Fir_Tree_01~08`（Nanite、无碰撞），噪声成林、避开陡坡与按交战朝向旋转的部署带（`FLegionOrientedBand`）；树冠写入寻路网格——密林格封锁寻路、疏林格给远程掩体（齐射 ×0.88 / ×0.75）；种子取当前战场种子，`-ForestSeed=N` 覆盖、`-NoForest` 关闭，种完自动校验三条车道仍连通。
* **开放世界局部随机战区与条件抽选 (`LegionGameMode` · `FLegionSpawnZone`)**：
  * 在 `Lvl_Legion`（`1008m × 1008m` 首颗行星）内置经离线扫描验证的 **8 个空间分离候选战区**（`东北缓谷` / `西翼盆地` / `北境台地` / `东南廊道` / `中央鞍部` / `东麓制高点` / `西北仰攻坡` / `西南深谷`，各带 `WalkableRatio`、`ReliefSpanCm`、`PlayerHighGroundDeltaCm` 与 `BattleAxisYawDeg`）；
  * 接取委托时依据 `TacticalProfile`、星区 `ConflictIntensity` 与 `DangerRating` 评分筛选 Top-3 候选池再按 `TerrainSeed` 确定性抽选；独立演武每次进场（或按 `R` 重开）随机换点；双方战线、延迟空降点、指挥官朝向、林地清空带与 `-LegionShot` 航拍机位围绕 `(DeployCenter, BattleAxisYawDeg)` 整体旋转；支持 `-LegionZone=0..7`、`-LegionSeed=N`、`-LegionProfile=0..4` 命令行覆盖。
* **动态气象与光影系统 (`LegionWeather.*`)**：
  * 内置 **6 套气象预设**（`清冽晨曦` / `烈阳正午` / `残阳暮色` / `山岚浓雾` / `阴云骤雨` / `战火硝烟`），按星区、冲突烈度与危险星级加权抽取（`-LegionWeather=0..5` 可强制指定）；
  * 运行时将关卡 `DirectionalLight` / `SkyLight` 切换为 `Movable` 并重新捕获天光（`RecaptureSky`），太阳方位围绕战线侧光旋转以雕琢山脊明暗；同步调控 `ExponentialHeightFog`、`PostProcessVolume` 饱和度与泛光、`ALandscapeProxy` 动态材质参数（若启用），并在暴雨/硝烟下由 `ALegionHUD` 绘制轻量屏幕雨丝或飘散余烬。
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

### 0. 分层设计铁律：3D 环境直挂纯白阴影排印 × 全屏终端「白底墨字彩条」× 禁止中英文并存

界面按**空间层级**严格区分两套视觉载体，使真实 3D 建筑/战场与全屏指挥终端各得其所：

**① 3D 环境直挂 HUD — 《控制：共振》式「高对比纯白文字 + 黑色错位立体阴影」**：
凡直接悬浮于真实 3D 场景之上的常驻 HUD（大厅标题与第一人称交互 HUD `UERHallHUD` / `UERHallButton`、军团战场 3D HUD `ULegionHUDWidget` / `ALegionHUD` / 右上「接入 AI」按钮、战终「胜利 / 失联」覆盖层），**一律取消白底卡片与暗色矩形盒**，将建筑空间与自然战场完整让渡给画面主体：
- **纯白排印 + 错位黑影立体感**：主体采用高纯度石膏白粗体汉字（`#FCFCFA`），底层叠加同字形黑色错位立体阴影（向右下偏移 `1.5×2` 至 `5×7 px`，阴影透明度 `0.80–0.90`），在强光石材、晴空云层、深色地面与草地阴影上均能形成锐利可读的浮雕对比；
- **巨细对比与垂直排印网格**：大厅左上 **`124 pt`** 巨型粗体「虹不我与」（`(5, 7)` 立体阴影）与左下 **`21 pt`** 紧凑菜单文字（「继续游戏 / 开始新游戏 / 读取存档 / 设置」）形成约 **6:1** 的巨幅尺度反差，上下文字左缘精准对齐同一垂直基线；悬停时左侧淡入带黑影的 `3×18 px` 纯白竖标并配合弹簧轻微右移，按压时阴影收紧至 `(1, 1.5)` 形成物理按陷反馈，禁用态以 `0.42` 透明度保持清晰；
- **工程统一入口**：UMG 文本调用 `ERUI::ApplyResonantText`（按字号自动匹配阴影偏移量），轻量按钮调用 `UERActionButton::SetResonantHudStyle(true, bLeftAlign)`，Canvas 世界标牌调用 `ALegionHUD::DrawResonantText`。

**② 全屏功能终端与面板 — 「白底墨字彩条」与暗曜石底幕**：
进入全屏/模态功能界面（虹膜指挥终端 `UERGalaCommandPanel`、全屏人员档案 `UERPersonnelPanel`、旧战斗面板）时，按钮与信息栏目保留高反差**「白底墨字彩条」**：反白石膏底 + 极墨黑粗体字 + 左侧 3–4px 《虹不我与》光谱条（或阵营/状态单色点睛条）+ 45° 切角 + 四角对准刻度（对应 `ERUI::MakeBoneCard` / `ERUI::StyleBoneFrame` / 默认 `UERActionButton`）；黑白等高线战略地图上的部队标牌采用极简暗曜石半透底板 + 纯白阴影字以适配高对比等高线底图。骨白底上的状态墨色须加深（危险 `(0.62, 0.04, 0.02)`、琥珀 `(0.58, 0.31, 0.0)`、冷蓝 `(0.02, 0.30, 0.62)`）。

**③ 禁止中英文同时出现**：同一控件（按钮、栏目、标题、副标、提示、番号牌）内**只允许中文**，不得再配英文副标、英文遥测码或「中文 // ENGLISH」式双语；大厅及战场直挂 HUD 已全部清理四角英文标尺与副标，仅允许保留阿拉伯数字、键盘键位标识（`E`、`M`、`Alt`、`Shift`、`WASD` 等）与第三方产品专名（如 `DEEPSEEK`）。
- 兵种/势力代号一律用中文简称（`ERGalacticMeta::GetClassCode` → 重盾/长戟/刃兵/射手），番号用旅级中文序数（`第一旅`、`第二旅`、`敌第一旅`、`自治领第一旅`）；货币单位写「银河币」。

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
| **Title / Metric** | `18–32 pt` | Bold，`+10 ~ +140` | 大厅菜单项（`21 pt` 纯白阴影）、面板标题、人员档案属性数字（`30 pt`）、终端按钮主字 |
| **Display 碑文** | `38–54 pt` | Bold，`+40 ~ +140` | 战场 `端点星`（`44 pt` 阴影碑文）、航线图 `航线图`（`42 pt`）与 `01 / 10`（`38 pt`）、战斗 `HP` 巨字（`52 pt`） |
| **Hero / Watermark** | `92–180 pt` | Bold，`+20 ~ +320` | 大厅 `虹不我与` 主标题碑（`124 pt` 纯白 + `(5, 7)` 错位黑影）、军团结算 `胜 利 / 失 联`（`180 pt`）、人员档案姓名（`92 pt`）与背景水印（`148 pt` @ 4.5%） |

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

### 5. 五大场景界面布局规范

1. **空间站大厅 (`WBP_HallHUD` / `WBP_HallButton`)**：全面采用《控制：共振》式无底卡纯白阴影排印——左上 `124 pt` 巨型粗体「虹不我与」（配 `(5, 7)` 黑色错位阴影）与 `13 pt` / `15 pt` 基金会标语形成 6:1 尺度反差；左下同垂直基线对齐的四枚 `21 pt` 无框按钮（「继续游戏 / 开始新游戏 / 读取存档 / 设置」），悬停仅亮起左侧 `3×18 px` 带阴影竖标并轻微右移；读取存档抽屉以 `1 px` 建筑竖线隔开，观景提示、存档提示与第一人称终端提示（`E · 接入虹膜指挥终端`）均去除了底盒与旧四角英文标尺，将大片日光建筑圣殿完整留白。
2. **军团战场 3D HUD (`ULegionHUDWidget` / `ALegionHUD` / `UERAICommandPanel`)**：左上 `44 pt`「端点星」配虹彩竖条；左侧兵团列表与左下敌我兵力以 `3 px` 阵营色竖标 + 纯白错位阴影文字呈现，选中兵团亮起暖金竖标并右移；右上势力 Logo 下方常驻无底卡「接入 AI · 就绪」切换钮；战场内我方头顶旗标、敌方高空通天标牌、准星集火提示均改用 `ALegionHUD::DrawResonantText` 绘制纯白阴影文字与贴底细血条，战终定格为 `180 pt` 极简纯白「胜利 / 失联」。
3. **全屏人员档案 (`WBP_PersonnelPanel` / `WBP_PersonnelMemberRow` / `WBP_PersonnelStatCell`)**：100% 全屏黑曜石底幕 + 顶部通栏 3px 虹彩窄条；**左半屏（0%–56%）全高立绘舞台**采用 `TC_EDITOR_ICON` 无压缩 2K 透明底原画（`IMG.PNG`），通过微负边距吃掉透明留白使人物与机甲顶天立地铺满左半屏且与背景零接缝；左下悬浮横向 4 干员切页坞（选中项反白高对比）；右半屏为 `92 pt` 姓名 + `148 pt` 淡水印 + `30 pt` 属性数字 + 四学派抗性色标与技能芯片。
4. **航线图 (`WBP_RouteHUD` / `WBP_RouteNode` / `WBP_RouteLegendItem`)**：严格执行航线图铁律——**全场仅玩家当前所在节点发光**（大范围辉光交由 3D `M_Route_Halo` 与 Slate 几何像素对齐），其余节点与连线一律不发光，连线只用细实线，不放零星粒子；左上 `42 pt` 粗体 `航线图` 配虹彩竖条，右上 `38 pt` Linux Biolinum 层级计数 `01 / 10`，底部建筑刻度标尺。
5. **早期回合制战斗 HUD 与 AI 战术终端 (`WBP_BattleHUD` / `UERAICommandPanel`)**：所有前景控件采用高对比哑光黑曜石切角底板，确保在明亮混凝土/自然岛战场上清晰锐利；左下 `HeroPlate` 呈现 `52 pt` 巨型 HP + 双层弹簧扣血条 + `18×5px` 细长 AP 格；底部 `98×132px` 切角战术手牌配反白费用章与底部学派色条；右上可折叠 **PRTS 风格 AI 战术终端**（`508px` 宽黑曜石切角框 + 左侧 3.5px 虹彩光谱条 + 单位索引 + `CMD/工具/RET/战术链路/ERR/SYS` 结构化标签日志 + 反白石膏主 `执行` 按钮）。

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
* **实现机制（双玩法双模挂载）**：
  * **大兵团战场等高线地图视角 (`Lvl_Legion`)**：进入地图视角后，**左上角挂载「接入 AI」按钮**与左侧停靠式 PRTS 终端（不影响地图视野与右上「返回战场」按钮，且悬停/输入时自动屏蔽地图缩放与快捷键）；玩家可输入如「全部部队优先合力进攻对方第一旅」「第二旅和第四旅往北高地迂回」等中文自然语言指令；服务自动注入全部敌我旅的实时 JSON 态势（番号/编号 `P1..Pn` 与 `E1..En`/兵种/存活人数/坐标/高程/当前命令）并暴露军团 Function Calling 工具（`order_attack`、`order_all_assault`、`order_move`、`order_move_relative`、`order_halt`、`order_follow_commander`、`select_allies`、`get_legion_battle_state`），支持宽松中文旅名模糊解析并在暂停的地图上即时画出所有下达的集火与机动命令线；
  * **旧战棋关卡 (`Lvl_ConcreteArena`)**：战斗 HUD 右上角挂载可折叠的 `AI 连接` 面板，将 3D 棋子状态（HP/AP、技能表、视线/掩体/高低差）与 Function Calling 工具表发送至外部大模型，异步驱动棋子寻路、施法与回合推进；
  * **统一协议与密钥安全**：兼容 OpenAI `/v1/chat/completions` 与 Anthropic `/v1/messages` 格式及多轮上下文管理；API 配置与 Key 仅保存在本地未跟踪目录 `Saved/ER_AIConfig.json`，不会进入 Git 仓库。

---

## 🎮 操作指南

### 军团大地图战场 (`Lvl_Legion`)
**战场视角（第一人称，默认）**
* **移动 / 视角**：`WASD` 移动、鼠标转向、`Shift` 疾跑、`空格` 跳跃；按住 `Alt` 临时呼出光标（点小地图、底部旅级卡片、按钮）；
* **左键**：纯射击（手枪），精准命中并击杀被射中的敌方士兵（按弹道方向触发布娃娃击飞）；
* **右键（全权指挥）**：点我方兵团选中（再点已唯一选中的兵团取消；`Ctrl` 增减选）、点地面机动（按住拖动 >350cm 可拉出阵型驻守朝向）；集火敌军时需**抬头瞄准敌方通天 Logo 右键**（避免地面机动误触）；`1 ~ 8` 快选；
* **军团指令**：`X` 停驻、`G` 全军突击、`F` 选中兵团（无选中则全军）跟随/取消跟随、`R` 重新部署、`ESC` 取消选中/关地图；

**等高线地图视角**（按住 `Alt` 点小地图或按 `M`；战场自动暂停，底部兵团卡自动隐藏）
* **缩放 / 平移**：滚轮以鼠标为中心缩放，左键拖拽或中键拖拽平移；
* **指挥**：左键点我方大 Logo 番号卡选中（`Ctrl`/`Shift` 增减），再点敌方番号集火，点空地（或右键）机动；
* **AI 自然语言指挥**：点击左上角「接入 AI」打开战术终端，配置密钥后可直接输入中文命令操控全军；
* **退出**：右上角「返回战场」按钮 / `M` / `ESC`；
* **自动化截图与自检**：`powershell -ExecutionPolicy Bypass -File Scripts/legion_shot.ps1`（五机位截图 + `ERMapSelfTest`）；控制台 `ERSkyBannerSelfTest`、命令行 `-SkyShot` / `-MapShot` 可验证通天 Logo、地图防碰撞与 AI 面板。

### 大厅关卡 (`Lvl_Hall`)
* **鼠标左键 / `Esc`**：第一人称漫游下按 `Esc` 释放光标，点击左部 **`01 兵团整备`** / **`02 银河委托`** / **`03 银河局势`** / **`04 设置`** 进入终端面板对应页签，`返回大厅` 退回；
* **命令行截图参数**：`-HallShot`（大厅主界面）、`-GalaShot`（默认截图银河委托页）。用 `-GalaShot -GalaTab=2 -LegionSeed=2026` 截取银河局势页；此组合使用内存战役，不读写正式存档。
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

### 大兵团银河战役（当前主线 · 近期进展与规划）

- [x] **首颗行星局部随机战区出生与局势条件抽选**：已在 `Lvl_Legion` 落地 8 个空间分离候选战区、任意交战轴向旋转（`FLegionOrientedBand` + `GetBattleBasis`），并接入委托 `TacticalProfile` + 星区 `ConflictIntensity` + `DangerRating` 评分筛选。
- [x] **动态天气与气象光照系统（首版）**：已落地 `FLegionWeatherSystem` 6 套预设、星区/烈度加权抽选、侧光太阳动态调谐、`ExponentialHeightFog` 雾效及 `ALegionHUD` 雨丝/余烬层。
- [ ] **局部战区边界视觉提示**：沿 `DeployCenter ± 160m` 四周补充克制的地面边界标识或 HUD 越界警示（目前角色与兵团坐标钳制已生效）。
- [ ] **三星球开放世界扩容与编辑器离线扫描器 (`ULegionArenaZoneCatalog`)**：制作另外两颗 `2km~4km`（`2017×2017` ~ `4033×4033`）Gaea 开放世界母图并将扫描打标固化为 `#if WITH_EDITOR` 资产生成工具。
- [ ] **复用 3D WFC 生成战区战术建筑 & 气象 Niagara 升级**：在平缓节点接入小尺度 `AWFC3DArenaHost` 掩体工事，并将 HUD 降水/余烬升级为世界空间 Niagara 粒子。

#### 军团 C++ 优化路线

以不改变战斗行为为前提。当前已完成地形寻路、部署与调试代码拆分；以下为后续计划，架构边界和采样方法见 [军团 C++ 架构说明](Docs/LegionSourceArchitecture.md)。

> **2026-12 方向修正**：按上文「士兵表现层技术选型铁律」，HISM 已禁用于战斗士兵表现，原第 1 条（HISM 变换更新优化）随之废止——骨骼路径成为唯一优化对象。已完成的 HISM 不变量 hoisting 保留在代码中不再演进，后续视骨骼路径实测决定是否删除整条实例化路径。

1. ~~优先优化实例化士兵（HISM）的变换更新~~（已废止，见上方方向修正）。
2. **拆分单兵模拟与表现写入**（已完成）：骨骼组件写入与 `DriveSoldierAnimation` 已移入 `WriteSkeletalSoldierRepresentation` 统一表现阶段。
3. **骨骼兵性能标尺已建成**（`-LegionPerf` + `-LegionForceSkeletal`，方法与基准见 [军团 C++ 架构说明](Docs/LegionSourceArchitecture.md)）：固定种子、全军突击、俯瞰最坏机位采样 10s 帧耗时并输出手工计时归因。
4. **游戏代码层优化已完成**（2026-12 二轮，行为经自检+近景截图回归验证）：
   - 命中模型纯数学化——逐兵物理命中胶囊永久废除（375 兵空烧 ~3.5ms/帧物理场景同步），玩家投射物/爆炸与士兵子弹统一走数学胶囊扫描（`ScanProjectileSoldierHit` / `ApplyExplosionToEnemySoldiers`），命中精度经 `[BulletHitSelfTest]` 验证不变；
   - 贴地静止收敛跳过（`ConformToTerrain`，~1.1ms）、士兵骨骼组件 `OnlyTickPoseWhenRendered`（视野外骨骼机器归零，第一人称实战收益大）、移动士兵变换写入按相机距离降频（50m+/120m+ 分带）、驻立士兵写入跳过、远距手枪隐藏。
   - **实测结论（192 兵/方俯瞰最坏）**：帧 35.8→29.1ms，game 线程 30.9→27.9ms，其中军团游戏代码仅 ~1.8ms（microAI 0.21 + skelRep 0.50 + bullets 0.22 + 团务）——**剩余 game 成本 ~93% 是引擎逐骨骼组件机器（骨骼矩阵收尾/上传/组件 tick 分发），游戏代码侧已无油水**。
5. **后续方向按此数据决策**：①再扩规模只有 ISKM 原型一条路（每兵一组件的引擎固定税不可逾越）；②GPU/渲染已上升为平行瓶颈（GPU ~26ms、空场景植被基线 ~17ms）——士兵阴影距离剔除、骨骼 LOD 减骨、植被/地形渲染预算属资产与设置层课题；③若既不扩规模也不做 ISKM，~150 总兵编制在第一人称实战可保 60fps（俯瞰最坏机位不代表真实游玩视角）。
6. **更大规模交火证明有收益后，再考虑空间索引**：原 384 兵样本中友军射界检查与弹道模拟分别约 **0.09** 和 **0.08 ms/帧**，优先级低；若引入索引，保留原有候选和命中顺序。
7. **单独测量进场耗时**：评估延后部署期的 `3072×3072` 战略图细节纹理烘焙，生成前用现有低一档纹理兜底。冷启动的资产注册表扫描与地图烘焙分别计时，不把启动等待计入战斗帧耗时。

**验收**：用相同种子、兵力和交战场景做前后对照，再开渲染检查移动、交火和死亡表现。无渲染数据只用于定位 CPU 开销，不代表实际渲染帧率或已取得的提速。

#### 🌍 场景技术路径：开放世界随机局部战区出生（3 星球 / 3 开放世界）

> **核心设计原则**：不在运行时强行程序化重塑 `ALandscape`（UE 5.8 原生 `Landscape` 高度图与碰撞在打包后只读，运行时自建网格又难以媲美 `MWLandscapeAutoMaterial` 与原生草丛质感），而是打造 **「少量高完成度开放世界母图 × 参数化局部战区切片 × 银河局势条件抽选」** —— 既保住远景群山、自动植被混合与 `ULegionDeployScreen::PrioritizeGrassCreation` 的流送性能，又让每次接取委托都像降落在全新战场。

1. **三星球 / 三开放世界母图规划（1 关卡 = 1 星球地貌生态）**
   - 用 Gaea 制作 3 张大尺度原生 `ALandscape` 世界（实测现有 `1km×1km` 约可容纳 8 个半重叠战区，后续星球建议直接采用 `2017×2017` ~ `4033×4033` 即 `2km~4km`，单场截取 `320m × 320m` 局部战区，四周天然环绕连绵远景群山）：
     - **星球一 · 温带高山草甸/针叶林**（当前 `Lvl_Legion` / `IslandExample` 风格，已跑通 8 点位局部随机）；
     - **星球二 · 寒带雪峰/苍白裂隙**（复用 `MTL_MWAM_Landscape_MountainRangeExample`）；
     - **星球三 · 荒漠矿带/戈壁台地**（复用 `MTL_MWAM_Landscape_DesertExample`）。
   - `UGalacticCampaignSubsystem::AcceptAndDeployContract` 按合同的 `TargetSectorId` / 星球生态路由 `OpenLevel` 至对应开放世界，6 大星区可共享并按各自氛围配置区分昼夜与雾效。

2. **战区生成点预生成（编辑器离线扫描与战术打标）**
   - 避免运行时盲随机坐标把部队刷在悬崖或死路中，通过离线扫描遍历开放世界，对所有候选 `(DeployCenter, BattleAxisYaw)`（8 方位）按与 `FLegionTerrainGrid` 一致的规则采样评估，后续固化为 `#if WITH_EDITOR` 工具生成 `ULegionArenaZoneCatalog` 资产：
     - **硬性准入校验**：我方/敌方部署带与延迟空降带的坡度合乎可通行阈值，且三条纵向战线车道经坡度/连通校验，自动淘汰绝壁/孤岛死点；
     - **量化战术特征标签**：自动计算每个候选战区的 `WalkableRatio`（开阔度）、`ReliefSpanCm`（垂直高差）、`PlayerHighGroundDeltaCm`（攻防单向高地差）等指标。

3. **银河局势与委托参数条件抽选（已实现首版）**
   - `ALegionGameMode::StartPlay()` 依据当前 `FGalacticContract` 与星区 `FGalacticSector` 动态评分筛选：
     - **委托战术倾向 (`ELegionTacticalProfile`)**：高地固守 / 仰攻夺高 / 峡谷伏击 / 开阔机动 / 均衡野战分别加权对应高差与开阔度区间；
     - **星区冲突烈度 (`ConflictIntensity`) / 危险星级 (`DangerRating`)**：高烈度/高星级优先拉高崎岖高差战区权重；
     - **确定性抽选**：取评分 **Top-3 候选池** 后由 `FRandomStream(Contract.TerrainSeed)` 抽取最终 `(DeployCenter, BattleAxisYawDeg)`，兼顾战术意图与种子可复现。

4. **战线轴向参数化与局部边界兼容（已实现首版）**
   - 已解除 `ALegionGameMode` 中 `DeployCenter` 取整张地形中心、战线固定 `-Y/+Y` 与相机固定 `InitialYawDeg = 90°` 的限制：用 `BattleAxisYawDeg` 构造局部 `(Forward, Right)` 基底布置我方/敌方战线、延迟空降点、指挥官朝向、`ALegionForest` 的 `FLegionOrientedBand` 清空区及三车道连通性校验。
   - **战略图映射零改造**：`FLegionTerrainGrid`、`WorldToMinimapUV`、`BuildMinimapPixels` 及 AI 战术终端保持“正北（+Y）朝上”不变，只随 `DeployCenter` 移动采样窗口；后续沿战区边缘补充可视化战术边界指示。

5. **动态天气与气象光照系统（已实现首版，待补全 Niagara 粒子）**
   - 按星球生态、星区 `ConflictIntensity`、`DangerRating` 与 `TerrainSeed` 加权抽取 6 套气象预设（`清冽晨曦`、`烈阳正午`、`残阳暮色`、`山岚浓雾`、`阴云骤雨`、`战火硝烟`）：
     - **光照与雾效**：程序化调节 `DirectionalLight`（围绕战线侧光旋转以雕琢山脊阴影）、`SkyLight`（`RecaptureSky`）、`ExponentialHeightFog`（雾密度、高度衰减与散射色）与 `PostProcessVolume`；
     - **降水与地表响应**：当前通过 `ALegionHUD` 绘制轻量雨丝/余烬与 `ALandscapeProxy::SetLandscapeMaterialScalarParameterValue` 联动（后续升级为相机跟随 Niagara 粒子并接入射程/视野规则）。

6. **复用 3D WFC 生成战区战术建筑与林地掩体（天然物理遮挡与寻路融合）**
   - **WFC 建筑模块复用**：复用项目既有的 3D WFC 框架（`AWFC3DArenaHost` / `WFC3DSolver` / `WFCKit` 与 `Futuristic` 材质套件），在非部署区的平缓高台或交通节点处以小规模 `Scale`（如 `3×3`~`6×6` 格）按 `TerrainSeed` 随机坍缩生成**前哨碉堡、矿区精炼塔、中继站遗迹、残垣工事**（底部加设贴合地形坡度的基座裙边防浮空），风格与规模受星区类型和工业化程度控制；
   - **自然植被与掩体**：同步叠加 **`ALegionForest` 种子化林地**（已实现）与 **巨石掩体群（`SM_MWAM_StoneA~D` HISM）**；
   - **玩法管线零割裂**：只要在 `FLegionTerrainGrid::Build()` 之前生成 `ECC_WorldStatic` 的 WFC 建筑碰撞，2D 坡度/障碍寻路网格、等高线战略图以及 `ALegionGameMode::SimulateBullets` 的弹道射线遮挡都会**自动识别这些建筑墙体作为真实掩体与绕行障碍**。

---

### 早期原型规划（3D WFC & 航线图）

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
