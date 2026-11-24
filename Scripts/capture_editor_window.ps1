param(
    [string]$Out = "Saved/Screenshots/editor_capture.png"
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class ERWin32 {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

[ERWin32]::SetProcessDPIAware() | Out-Null

$proc = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $proc) {
    Write-Error "UnrealEditor main window not found."
    exit 1
}

$hWnd = $proc.MainWindowHandle
[ERWin32]::ShowWindow($hWnd, 9) | Out-Null
[ERWin32]::SetForegroundWindow($hWnd) | Out-Null
Start-Sleep -Milliseconds 800

$rect = New-Object ERWin32+RECT
[ERWin32]::GetWindowRect($hWnd, [ref]$rect) | Out-Null
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top

Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bmp)
$graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
$graphics.Dispose()

$dir = Split-Path -Parent $Out
if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Output "$Out ${width}x${height} at $($rect.Left),$($rect.Top)"
