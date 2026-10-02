# 用 Inno Setup 把 dist/ 打成安装包。
# 先运行 build.ps1 构建 dist/，再运行本脚本：  .\make-installer.ps1
cd 'E:\Projects\Hello KSP Launcher'

$iscc = 'C:\Users\Zhuwenqian\AppData\Local\Programs\Inno Setup 7\ISCC.exe'
$iss = 'installer\helloksplauncher.iss'

# --- 版本号取自 src/appversion.h（单一来源），自动改写 .iss 的 #define MyAppVersion
$ver = (Select-String -Path src\appversion.h -Pattern '#define\s+HKSPL_APP_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value
if (-not $ver) { Write-Error '无法从 src/appversion.h 读取版本号'; exit 1 }
Write-Host "[installer] version = $ver"

# 必须显式 -Encoding UTF8：PS5.1 默认按 ANSI 读无 BOM 的 UTF-8 会把中文读坏
(Get-Content $iss -Raw -Encoding UTF8) -replace '#define MyAppVersion "[^"]*"', "#define MyAppVersion `"$ver`"" | Set-Content $iss -NoNewline -Encoding UTF8

# --- 编译安装包（输出到项目根目录）
& $iscc $iss
if ($LASTEXITCODE -ne 0) { Write-Error 'ISCC 编译失败'; exit 1 }

Get-Item "HelloKSPLauncher-$ver-setup.exe" | Select-Object Name, Length, LastWriteTime
