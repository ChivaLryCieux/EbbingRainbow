# 启动 -game 进航线图, 等 -RouteShot 自动截图后关进程, 打印截图路径。
# 用法: powershell -File Scripts/route_shot.ps1 [-Seed 12345] [-ResX 1920] [-ResY 1080] [-Extra "-Atmo"]
param(
    [int]$Seed = 424242,
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
    "/Game/Battle/Maps/Lvl_Route?seed=$Seed",
    "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-ForceRes", "-RouteShot", "-log"
) + ($Extra -split " " | Where-Object { $_ })
$p = Start-Process "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $args -PassThru

$deadline = (Get-Date).AddSeconds(240)
$found = $null
while ((Get-Date) -lt $deadline -and -not $found) {
    Start-Sleep 3
    if (Test-Path $shots) {
        $found = Get-ChildItem $shots -File | Where-Object { -not $before.ContainsKey($_.Name) } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
}
Start-Sleep 2
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
if ($found) { Write-Output $found.FullName } else { Write-Output "NO SHOT (timeout)" }
