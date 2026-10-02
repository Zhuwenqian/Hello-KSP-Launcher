# Configure + build (MinGW, Qt6). Update $qtDir / $mingw after Qt upgrades.
cd 'E:\Projects\Hello KSP Launcher'
$qtDir = 'E:\Qt\6.12.0\mingw_64'
$mingw = 'E:\Qt\Tools\mingw1310_64\bin'
$env:Path = "$mingw;$qtDir\bin;" + $env:Path
$cmake = 'E:\Qt\Tools\CMake_64\bin\cmake.exe'

& $cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$qtDir" "-DCMAKE_MAKE_PROGRAM=$mingw\mingw32-make.exe" "-DCMAKE_C_COMPILER=$mingw\gcc.exe" "-DCMAKE_CXX_COMPILER=$mingw\g++.exe"
if ($LASTEXITCODE -ne 0) { exit 1 }

& $cmake --build build --target HelloKSPLauncher -j4
if ($LASTEXITCODE -ne 0) { exit 1 }

# Deploy Qt runtime (DLLs + plugins) to dist so it matches the Qt we built
# against; stale Qt6*.dll in dist cause "entry point not found" after upgrades.
& "$qtDir\bin\windeployqt.exe" --no-translations dist\HelloKSPLauncher.exe
if ($LASTEXITCODE -ne 0) { exit 1 }
# windeployqt only sees the exe's direct imports: add what libckan.dll needs.
Copy-Item "$qtDir\bin\Qt6Concurrent.dll" dist -Force

Get-Item 'dist\HelloKSPLauncher.exe' | Select-Object Name, Length, LastWriteTime
