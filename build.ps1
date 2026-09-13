cd 'E:\Projects\Hello KSP Launcher'
$env:Path = "E:\Qt\Tools\mingw1310_64\bin;" + $env:Path
$p = Start-Process -FilePath 'E:\mingw64\bin\cmake.exe' -ArgumentList '--build','.','--target','HelloKSPLauncher','-j4' -WorkingDirectory 'E:\Projects\Hello KSP Launcher\build' -NoNewWindow -Wait -RedirectStandardOutput 'bout.txt' -RedirectStandardError 'berr.txt'
Write-Output "--- STDOUT (last 12) ---"; Get-Content bout.txt | Select-Object -Last 12
Write-Output "--- STDERR (last 15) ---"; Get-Content berr.txt | Select-Object -Last 15
Write-Output "exe:"; Get-Item "E:\Projects\Hello KSP Launcher\dist\HelloKSPLauncher.exe" | Select-Object Name, Length, LastWriteTime
