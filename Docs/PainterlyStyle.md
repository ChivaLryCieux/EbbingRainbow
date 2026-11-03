# 厚涂手绘（Impasto Painterly）渲染风格

> 用一份屏幕空间着色器，把 UE5.8 的实时画面整体转成「厚涂油画 / 手绘」观感：
> 扁平化色块、沿结构流动的笔触、真实的颜料堆叠高光、手绘墨线、画布纹理。

---

## 1. 交付内容

| 文件 | 作用 |
| --- | --- |
| `Shaders/Painterly/PainterlyCommon.ush` | 全部算法：噪声、方向场、各向异性 Kuwahara、堆料法线、墨线、画布、调色 |
| `Shaders/Painterly/PainterlyPostProcess.ush` | 参数结构体 `FPaintParams` + 入口 `Paint_Main()` |
| `Scripts/Painterly/build_painterly_postprocess.py` | 在编辑器里一键生成后处理材质 + 3 个预设材质实例 |
| `Scripts/Painterly/smoke_test_painterly.py` | 无人值守冒烟测试：启动游戏、设状态、截图、退出 |
| `Scripts/Painterly/analyse_painterly_shots.py` | 量化对比 on/off，输出指标与对比图 |
| `Scripts/Painterly/verify_painterly_assets.py` | 校验生成的材质资产（域、引脚、参数、连线） |
| `Source/EbbingRainbow/Rendering/PainterlyStyleSubsystem.*` | 自动给所有关卡挂载后处理、并提供 `Paint.Style` 开关 |
| `Source/EbbingRainbow/EbbingRainbow.cpp` | 注册虚拟着色器目录 `/Project` → `Shaders/`（让 Custom 节点能 `#include`） |
| `Config/DefaultGame.ini` | `[/Script/EbbingRainbow.PainterlyStyle] MaterialPath=...` |

生成出来的资产：

```
/Game/Style/Painterly/M_PP_Painterly          后处理母材质（Material Domain = Post Process）
/Game/Style/Painterly/MI_PP_Painterly         默认厚涂预设
/Game/Style/Painterly/MI_PP_Painterly_Ink     墨线更重、色阶更少（版画/速写味）
/Game/Style/Painterly/MI_PP_Painterly_Soft    笔刷更大更软、更湿润（水粉/水彩味）
```

---

## 2. 画面是怎么「变厚」的

整条管线在一次后处理 pass 里完成（Blendable Location = **After Tonemapping**，UI 不受影响）。

### 2.1 笔触方向场（Flow）

* 3×3 亮度 Sobel 得到梯度 `g`；
* **边缘方向** = `g` 旋转 90°，用**倍角向量** `(cos2θ, sin2θ)` 表示，避免 0°/180° 反向抵消；
* **平坦区**（墙面、天空、地面）没有可靠梯度，改用一个低频噪声流的切线方向；
* 两者按 `Confidence`（梯度/局部对比度）混合，再加一点逐像素抖动，笔触才不会像机械网格。

> 这一步决定「笔往哪儿走」。厚涂最怕的就是方向场糊成一团，所以平坦区必须给一个**连贯**的方向，而不是让它随机。

### 2.2 各向异性 Kuwahara（把画面拍成色块）

沿笔触方向拉长的 8 个扇区，每个扇区 5 个采样点，统计亮度方差，用

```
w = 1 / (1 + Var / VarRef · 6) ^ Patchiness
```

加权求均值。`Patchiness` 越大越接近经典 Kuwahara（只取最平的扇区 → 硬边色块）；越小越像湿画法的柔化。这是「厚涂感」的主体。

### 2.3 堆料光照（颜料真的有厚度，但画面不变暗）

沿笔触方向拉长的脊状 fbm 当作高度场 `h`（波长约「横向 12px / 纵向 55px」，正好是一个笔刷的宽长比）。

光照**不是**用 `N·L`。第一版就是 `Shade = dot(N, L) / L.z`，理论上平面处等于 1，但实测整幅画面掉了 25% 亮度：每个背光的小面都会变暗，而朝光的小面因为 `1/√(1+s²)` 的非线性并不能等量补回来。

现在的做法是取高度场沿光方向的**方向导数**：

```hlsl
Slope = ( h(P + L) - h(P - L) ) / 2
Shade = 1 + Slope * Relief        // 均值恒为 0
```

平稳噪声场的导数均值严格为 0，所以无论 `Relief` 多大，画面平均亮度都不漂移（实测 +0.5%）。高光则直接取脊顶 `pow(h, 12)`，只有颜料脊线才会亮起来。

因为光照方向是**跟着笔触切向**旋转的，高光会顺着笔触拉成长条——这才是油画的「堆料反光」，而不是一层均匀的浮雕噪声。

### 2.4 墨线（手绘轮廓）

三种独立的边：颜色梯度、**深度不连续**（相对深度，任何距离都成立）、**法线断裂**。取最大值后，用低频噪声做**抖动阈值**（线宽忽粗忽细）+ 干笔破碎（线条断续），所以不会像 Sobel 滤镜那样死板。

这里有个坑值得记一下：颜色边最初写成 `|∇L| / 局部对比度`，这是**尺度无关**的，于是画面里每一点微弱纹理都会被判成「边」——实测墨线覆盖率 15.6%，整幅画面被压暗 18%，看起来像版画而不是油画。现在乘了一个绝对对比度闸门 `smoothstep(0.03, 0.16, 局部对比度)`，并且把深度/法线边的增益降下来，只有真正的轮廓才吃墨，覆盖率降到 4% 左右。

### 2.5 画布 + 调色

* 交叉正弦 + 噪声做画布织纹，轻微乘到颜色上；
* **只对亮度做色阶化**（保色相），避免直接 posterize 导致脏色；
* 冷暖分离 + 暗角，把整体压到「颜料」而不是「CG」的色域里。

---

## 3. 使用步骤

### 3.1 编译 C++

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" EbbingRainbowEditor Win64 Development `
    -Project="D:\EbbingRainbow\EbbingRainbow.uproject" -WaitMutex
```

（或在编辑器里直接点 Compile / 用 Live Coding。改动涉及新增 cpp，建议完整构建一次。）

### 3.2 生成材质

任选一种：

* **编辑器 Python 控制台**（Output Log 切到 Python）：

  ```python
  exec(open(r"D:/EbbingRainbow/Scripts/Painterly/build_painterly_postprocess.py").read())
  ```

* **菜单** Tools → Execute Python Script… → 选该文件

* **命令行**（不需要打开界面）：

  ```powershell
  & "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
      "D:\EbbingRainbow\EbbingRainbow.uproject" `
      -run=pythonscript `
      -Script="D:\EbbingRainbow\Scripts\Painterly\build_painterly_postprocess.py" `
      -unattended -nosplash -nop4
  ```

### 3.3 挂载

**默认已经自动挂载**：`UPainterlyStyleSubsystem` 在每个游戏/PIE 世界 `BeginPlay` 时生成一个 unbound 的 PostProcessVolume，把 `MI_PP_Painterly` 塞进去。大厅 / 航线 / 战场三个关卡都会生效，打包后也生效。

运行时开关与调试命令（都可以在 PIE / 打包后的控制台里用）：

```
Paint.Style 0                  # 原样渲染，方便对比
Paint.Style 1                  # 厚涂
Paint.DebugMode 3              # 调试视图：0 最终 / 1 方向场 / 2 墨线遮罩 / 3 堆料法线 / 4 纯 Kuwahara / 5 边缘置信度
Paint.Param P_BrushRadius 12   # 运行时改任意参数，不用重编材质
Paint.Shot my_shot             # 截一帧到 Saved/Screenshots/my_shot.png
```

材质是通过 `UMaterialInstanceDynamic` 挂上去的，所以 `Paint.Param` / `Paint.DebugMode` 立刻生效——调参时建议开 `Paint.DebugMode 1` 看笔触方向、`3` 看厚度、`2` 看墨线是不是太多。

想手动控制（比如只在战斗关卡开、或者不同关卡用不同预设）：
把 `Config/DefaultGame.ini` 里的

```ini
[/Script/EbbingRainbow.PainterlyStyle]
MaterialPath=/Game/Style/Painterly/MI_PP_Painterly
```

改成别的实例，或者把 `MaterialPath=` 清空，然后自己在关卡里放 PostProcessVolume、把实例拖进 *Post Process Materials*（记得勾 **Infinite Extent (Unbound)**）。

> **打包注意**：材质只通过字符串路径被引用，cooker 扫不到它。`DefaultGame.ini` 里已经加了
> `+DirectoriesToAlwaysCook=(Path="/Game/Style/Painterly")` 强制打进包里；如果你把材质挪到别的目录，记得同步改这一行。

---

## 4. 参数表

| 参数 | 默认 | 范围 | 说明 |
| --- | --- | --- | --- |
| `P_BrushRadius` | 7.0 | 2–20 | 单笔覆盖半径（像素）。越大越「大笔触」，也越糊 |
| `P_StrokeStretch` | 0.55 | 0–1 | 滤波器沿笔触方向的拉长程度 |
| `P_Patchiness` | 2.2 | 0.5–8 | 色块硬朗度。1 附近＝湿画柔化，4+ ＝硬边色块 |
| `P_TensorRadius` | 1.6 | 0.5–4 | 方向场梯度采样半径（像素） |
| `P_FlowFreq` | 5.0 | 1–20 | 平坦区流动噪声的频率（越大越碎） |
| `P_FlowJitter` | 0.35 | 0–1 | 笔触方向抖动 |
| `P_ImpastoStrength` | 0.55 | 0–1 | 堆料立体感强度 |
| `P_ImpastoAngle` | 3.927 | 弧度 | 假想光方向。3.927 ≈ 225°（左上方来光，屏幕 y 轴向下） |
| `P_ImpastoScale` | 1.0 | 0.2–4 | 笔刷纹理密度 |
| `P_CanvasStrength` | 0.35 | 0–1 | 画布纹理 |
| `P_InkStrength` | 0.50 | 0–1 | 墨线强度 |
| `P_InkWidth` | 1.0 | 0.2–4 | 墨线粗细（同时影响边缘采样半径） |
| `P_QuantSteps` | 12 | 2–32 | 亮度色阶数 |
| `P_QuantStrength` | 0.35 | 0–1 | 色阶化强度 |
| `P_Saturation` | 1.06 | 0.5–1.5 | 饱和度 |
| `P_Warmth` | 0.25 | -1–1 | 冷暖分离（负=冷，正=暖） |
| `P_Vignette` | 0.14 | 0–1 | 暗角 |
| `P_DebugMode` | 0 | 0–5 | 调试视图，见下 |

**调试视图**（`P_DebugMode`）：`1` 方向场，`2` 墨线遮罩，`3` 堆料法线，`4` 纯 Kuwahara，`5` 边缘置信度。
调参时先看 `1`（笔触是不是顺着结构走）、再看 `3`（厚度感够不够），最后看 `2`（线是不是太多）。

---

## 5. 怎么验证它真的对了

滤镜类的改动最怕「看着好像变了」但其实是变糊/变暗。这里给一套**能量指标**，跑一遍就能判断：

| 指标 | 含义 | 期望 |
| --- | --- | --- |
| `mean` | 平均亮度 | 漂移 < ±8%（否则玩家会觉得关卡变暗了） |
| `colors` | 5bit 量化后的唯一色数 | 明显下降（色块化） |
| `hf` | 1px 尺度梯度能量 | 小幅变化；暴涨说明变成噪点 |
| `brush` | 4px 尺度带通能量 | 上升（笔触/堆料纹理） |
| 低频相关 | 与原图 16px 模糊后的相关系数 | 0.7+（结构没被破坏） |

复现方式（每次运行只跑一种状态，避免状态切换与截图之间的竞态）：

```powershell
$env:PAINT_STYLE=0; $env:PAINT_SHOT="painterly_off"
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\EbbingRainbow\EbbingRainbow.uproject" `
    -game -windowed -ResX=1600 -ResY=900 `
    -ExecCmds="py D:/EbbingRainbow/Scripts/Painterly/smoke_test_painterly.py" -log

& python "D:\EbbingRainbow\Scripts\Painterly\analyse_painterly_shots.py"
```

`analyse_painterly_shots.py` 会打印指标、两张 ASCII 缩略图，并输出
`Docs/painterly_compare.png`（左原图/右厚涂）、`painterly_detail.png`（3 倍放大）、`painterly_diff.png`（差异热图）。

### 本机实测（1600×900，竞技场关卡）

| 版本 | mean | colors | hf | brush |
| --- | --- | --- | --- | --- |
| 原图 | 0.617 | 707 | 0.0255 | 0.0105 |
| 厚涂 | 0.584 | 590 | 0.0522 | 0.0242 |
| 变化 | **−5%** | **−17%** | +105% | **+129%** |

拆解诊断（每次只开一层，用 `Paint.Param` 把其它参数清零）：

| 只开 | mean | 结论 |
| --- | --- | --- |
| 纯 Kuwahara | 0.617 | 滤波本身完全不改变亮度 ✅ |
| 只开堆料光照 | 0.622 | 方向导数光照亮度中性（+0.5%）✅ |
| 只开墨线 | 0.553 | **第一版墨线独自压暗 10.6%** ❌ → 加对比度闸门后降到 4% 覆盖率 |
| 只开调色/暗角 | 0.595 | −3.9%，属于有意为之的暗角 |

这张表就是这次调参的全部依据：**先证明每一层单独是中性或有意的，再看合成结果**。

---

## 6. 性能

单 pass，采样数随半径固定：

```
方向场 8 tap + Kuwahara 8×5 = 40 tap + 墨线 9 tap ≈ 57 tap/像素
```

* 1080p：主流独显 ~1–2 ms；
* 4K：约 4 倍，可能需要 `P_BrushRadius` 下调或配合 `r.ScreenPercentage`；
* 想再省：把 `Paint_Kuwahara` 里的 `STEPS` 从 5 降到 3（约省 30%），或把 `SECTORS` 降到 6。

---

## 7. 已知限制 / 可以继续做的

1. **屏幕空间**：笔触图案固定在屏幕上，镜头快速平移时会有轻微「游动」。本作是回合制，问题不大；若要彻底解决，需要把方向场投影到世界空间（用 SceneDepth 反投影）或加一层速度自适应的笔触淡出（`SceneTexture:Velocity`）。
2. **Forward / Mobile 不支持**：`SceneTextureLookup` 只在 Deferred 下存在。工程是 Windows/DX12 桌面，没问题；如果要上移动端，需要改走 `MobileSceneTextureLookup` 或用降级分支。
3. **只作用于不透明物体**：半透明（玻璃、粒子、UI）不经过这套滤镜。需要的话可以给半透明材质单独做一套「手绘描边」材质。
4. **可以进一步做的**：
   * 把 `Paint_BrushHeight` 复用到**表面材质**（世界空间 UV），让近景物体本身有笔触起伏，而不只是屏幕滤镜；
   * 给不同关卡/区域做不同预设（大厅干净一点、战场脏一点），用 PostProcessVolume 的 Blend Weight 做过渡；
   * 加一层「湿边」（颜料在色块交界处堆积加深），`Paint_Kuwahara` 里已有方差信息，可以直接复用。
