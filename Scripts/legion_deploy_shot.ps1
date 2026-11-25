# 启动 -game 进军团演武场, 走 -DeployShot 接驳屏三态连拍 (加载中 -> 就绪 -> 进入战场) 后关进程, 打印路径。
# 验证「出击部署加载屏」: 草皮构筑/资产流送在屏后完成, 进入战场时草已铺满。
# 用法: powershell -File Scripts/legion_deploy_shot.ps1 [-ResX 1920] [-ResY 1080] [-Extra ""]
param(
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
    "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-DeployShot", "-log"
)
$args += ($Extra -split " " | Where-Object { $_ })
$p = Start-Process "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $args -PassThru

$deadline = (Get-Date).AddSeconds(180)
$found = @()
while ((Get-Date) -lt $deadline -and $found.Count -lt 3) {
    Start-Sleep 3
    if (Test-Path $shots) {
        $found = @(Get-ChildItem $shots -File | Where-Object { -not $before.ContainsKey($_.Name) } | Sort-Object LastWriteTime)
    }
}
Start-Sleep 4
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
if ($found.Count -gt 0) { $found | ForEach-Object { Write-Output $_.FullName } } else { Write-Output "NO SHOT (timeout)" }
