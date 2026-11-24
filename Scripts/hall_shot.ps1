# 启动 -game 进大厅, 等待加载后整屏截图 (大厅没有内置自动截图), 然后关进程。
# 用法: powershell -File Scripts/hall_shot.ps1 [-WaitSec 45] [-Out Saved/Screenshots/HallShot.png]
param(
    [int]$WaitSec = 45,
    [string]$Out = "D:\EbbingRainbow\Saved\Screenshots\HallShot.png"
)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class Dpi { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); }'
[Dpi]::SetProcessDPIAware() | Out-Null
$args = @("D:\EbbingRainbow\EbbingRainbow.uproject", "/Game/Battle/Maps/Lvl_Hall",
    "-game", "-windowed", "-ResX=1920", "-ResY=1080", "-ForceRes", "-WinX=0", "-WinY=0", "-log")
$p = Start-Process "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $args -PassThru
Start-Sleep $WaitSec
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Write-Output $Out
