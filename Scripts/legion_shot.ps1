# 启动 -game 进军团演武场, 等 -LegionShot 的五张自动截图 (开局第一人称 + 交战俯拍 + 回到第一人称 + 等高线地图视角 + 命令反馈) 后关进程, 打印路径。
# 用法: powershell -File Scripts/legion_shot.ps1 [-Legions 4] [-Soldiers 10] [-ResX 1920] [-ResY 1080] [-Extra ""]
#   压测例: powershell -File Scripts/legion_shot.ps1 -Legions 8 -Soldiers 125  (共 2000 单位)
param(
    [int]$Legions = 0,
    [int]$Soldiers = 0,
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [string]$Extra = ""
)
$ErrorActionPreference = "Stop"
$shots = "D:\EbbingRainbow\Saved\Screenshots\WindowsEditor"
$before = @{}
if (Test-Path $shots) { Get-ChildItem $shots -File | ForEach-Object { $before[$_.Name] = $true } }

$args = @(
    "D:\EbbingRainbow\EbbingRainbow.uproject",
    "/Game/Battle/Maps/Lvl_Legion",
    "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-LegionShot", "-log"
)
if ($Legions -gt 0)  { $args += "-LegionLegions=$Legions" }
if ($Soldiers -gt 0) { $args += "-LegionSoldiers=$Soldiers" }
$args += ($Extra -split " " | Where-Object { $_ })
$p = Start-Process "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $args -PassThru

$deadline = (Get-Date).AddSeconds(240)
$found = @()
while ((Get-Date) -lt $deadline -and $found.Count -lt 5) {
    Start-Sleep 3
    if (Test-Path $shots) {
        $found = @(Get-ChildItem $shots -File | Where-Object { -not $before.ContainsKey($_.Name) } | Sort-Object LastWriteTime)
    }
}
Start-Sleep 6
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
if ($found.Count -gt 0) { $found | ForEach-Object { Write-Output $_.FullName } } else { Write-Output "NO SHOT (timeout)" }
