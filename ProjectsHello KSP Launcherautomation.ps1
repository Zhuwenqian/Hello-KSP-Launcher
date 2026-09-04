# KSP 启动器 UI 诊断自动化脚本
Add-Type -ReferencedAssemblies @("System.Drawing", "System.Windows.Forms") -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Drawing;
using System.Drawing.Imaging;
public class Win32Helper {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int X, int Y);
    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, int dx, int dy, uint cButtons, IntPtr dwExtraInfo);
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hDC, uint nFlags);
    [DllImport("gdi32.dll")]
    public static extern IntPtr CreateCompatibleDC(IntPtr hDC);
    [DllImport("gdi32.dll")]
    public static extern IntPtr CreateCompatibleBitmap(IntPtr hDC, int nWidth, int nHeight);
    [DllImport("gdi32.dll")]
    public static extern IntPtr SelectObject(IntPtr hDC, IntPtr hObject);
    [DllImport("gdi32.dll")]
    public static extern bool DeleteDC(IntPtr hdc);
    [DllImport("gdi32.dll")]
    public static extern bool DeleteObject(IntPtr hObject);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    public static void Click(int x, int y) {
        SetCursorPos(x, y);
        System.Threading.Thread.Sleep(50);
        mouse_event(0x0002 | 0x0004, 0, 0, 0, IntPtr.Zero);
        System.Threading.Thread.Sleep(50);
    }
    public static void ScreenshotWindow(IntPtr hwnd, string savePath) {
        RECT rect;
        GetWindowRect(hwnd, out rect);
        int width = rect.Right - rect.Left;
        int height = rect.Bottom - rect.Top;
        if (width <= 0 || height <= 0) return;
        try {
            using (Bitmap bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
                using (Graphics g = Graphics.FromImage(bmp)) {
                    IntPtr hdc = g.GetHdc();
                    IntPtr memDc = CreateCompatibleDC(hdc);
                    IntPtr hBitmap = CreateCompatibleBitmap(hdc, width, height);
                    IntPtr oldBitmap = SelectObject(memDc, hBitmap);
                    PrintWindow(hwnd, memDc, 0);
                    SelectObject(memDc, oldBitmap);
                    DeleteDC(memDc);
                    g.ReleaseHdc(hdc);
                    using (Bitmap result = Image.FromHbitmap(hBitmap)) {
                        result.Save(savePath, ImageFormat.Png);
                    }
                    DeleteObject(hBitmap);
                }
            }
        } catch (Exception ex) { Console.WriteLine("Screenshot error: " + ex.Message); }
    }
}
"@ -ErrorAction Stop
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
Write-Output "停止旧进程..."
Get-Process -Name "HelloKSPLauncher" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
$exePath = "e:ProjectsHello KSP LauncherdistHelloKSPLauncher.exe"
Write-Output ("启动程序: " + $exePath)
$proc = Start-Process $exePath -PassThru -ErrorAction Stop
Write-Output ("PID: " + $proc.Id)
Write-Output "等待窗口启动..."
$maxWaitSec = 20
$waited = 0
$hwnd = [IntPtr]::Zero
while ($waited -lt $maxWaitSec) {
    Start-Sleep -Seconds 1
    $waited++
    try {
        $proc.Refresh()
        if (-not $proc.HasExited -and $proc.MainWindowHandle -ne [IntPtr]::Zero) {
            $hwnd = $proc.MainWindowHandle
            Write-Output ("窗口已出现! 等待: " + $waited + "s")
            break
        }
    } catch {}
}
if ($hwnd -eq [IntPtr]::Zero) { Write-Output "FAIL: No window"; exit 1 }
$rect = New-Object Win32Helper+RECT
[Win32Helper]::GetWindowRect($hwnd, [ref]$rect)
$winW = $rect.Right - $rect.Left
$winH = $rect.Bottom - $rect.Top
Write-Output ("窗口: " + $proc.MainWindowTitle)
Write-Output ("大小: " + $winW + "x" + $winH + " pos:" + $rect.Left + "," + $rect.Top)
[Win32Helper]::ShowWindow($hwnd, 9) | Out-Null
Start-Sleep -ms 300
[Win32Helper]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -ms 800
$shot1 = "e:ProjectsHello KSP Launchershot_01_main.png"
Write-Output ("截图1: " + $shot1)
[Win32Helper]::ScreenshotWindow($hwnd, $shot1)
Write-Output "点击设置按钮..."
Add-Type -AssemblyName System.Windows.Forms
$candidates = @(
    @($rect.Left + 45, $rect.Top + [int]($winH * 0.88)),
    @($rect.Left + 45, $rect.Top + [int]($winH * 0.80)),
    @($rect.Left + 45, $rect.Top + [int]($winH * 0.72)),
    @($rect.Left + 45, $rect.Top + [int]($winH * 0.65)),
    @($rect.Left + 60, $rect.Top + [int]($winH * 0.75)),
    @($rect.Left + 80, $rect.Top + [int]($winH * 0.75))
)
foreach ($p in $candidates) {
    [Win32Helper]::Click($p[0], $p[1])
    Start-Sleep -ms 400
    [Win32Helper]::SetForegroundWindow($hwnd) | Out-Null
}
Start-Sleep -ms 800
[Win32Helper]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -ms 300
[Win32Helper]::GetWindowRect($hwnd, [ref]$rect)
$defW = $rect.Right - $rect.Left
$defH = $rect.Bottom - $rect.Top
$shot2 = "e:ProjectsHello KSP Launchershot_02_settings_default.png"
Write-Output ("截图2: " + $shot2)
[Win32Helper]::ScreenshotWindow($hwnd, $shot2)
Write-Output ("默认尺寸: " + $defW + "x" + $defH)
$cx = $rect.Left + [int]($defW * 0.55)
$cy = $rect.Top + [int]($defH * 0.6)
[Win32Helper]::Click($cx, $cy)
Start-Sleep -ms 200
for ($i = 0; $i -lt 5; $i++) { [System.Windows.Forms.SendKeys]::SendWait("{PGDN}"); Start-Sleep -ms 200 }
Start-Sleep -ms 500
[Win32Helper]::SetForegroundWindow($hwnd) | Out-Null
$shot3 = "e:ProjectsHello KSP Launchershot_03_mod_default.png"
Write-Output ("截图3: " + $shot3)
[Win32Helper]::ScreenshotWindow($hwnd, $shot3)
Write-Output "最大化..."
[Win32Helper]::ShowWindow($hwnd, 3) | Out-Null
Start-Sleep -ms 1200
[Win32Helper]::GetWindowRect($hwnd, [ref]$rect)
$maxW = $rect.Right - $rect.Left
$maxH = $rect.Bottom - $rect.Top
Write-Output ("最大化: " + $maxW + "x" + $maxH)
[Win32Helper]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -ms 500
$shot4 = "e:ProjectsHello KSP Launchershot_04_settings_max.png"
Write-Output ("截图4: " + $shot4)
[Win32Helper]::ScreenshotWindow($hwnd, $shot4)
$cx2 = $rect.Left + [int]($maxW * 0.55)
$cy2 = $rect.Top + [int]($maxH * 0.55)
[Win32Helper]::Click($cx2, $cy2)
Start-Sleep -ms 200
for ($i = 0; $i -lt 5; $i++) { [System.Windows.Forms.SendKeys]::SendWait("{PGDN}"); Start-Sleep -ms 200 }
Start-Sleep -ms 500
$shot5 = "e:ProjectsHello KSP Launchershot_05_mod_max.png"
Write-Output ("截图5: " + $shot5)
[Win32Helper]::ScreenshotWindow($hwnd, $shot5)
Write-Output "关闭程序..."
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
Write-Output ""
Write-Output "==== 诊断完成 ===="
Write-Output ("默认窗口: " + $defW + "x" + $defH)
Write-Output ("最大化: " + $maxW + "x" + $maxH)
