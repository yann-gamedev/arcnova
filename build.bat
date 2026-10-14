@echo off
setlocal EnableExtensions
cd /d "%~dp0" || exit /b 1

where windres >nul 2>nul || (echo [ArcNova] windres not found in PATH.& exit /b 1)
where g++ >nul 2>nul || (echo [ArcNova] g++ not found in PATH.& exit /b 1)

echo [ArcNova] Compiling icon resource...
windres assets/resource.rc -O coff -o assets/resource.o || exit /b 1

echo [ArcNova] Compiling ArcNova.exe...
g++ -std=c++17 -m64 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -finput-charset=UTF-8 -fexec-charset=UTF-8 -Iinclude src/main.cpp src/core/*.cpp src/commands/*.cpp assets/resource.o -o ArcNova.exe -static -static-libgcc -static-libstdc++ -s -ladvapi32 || exit /b 1

echo [ArcNova] Build complete: "%CD%\ArcNova.exe"
exit /b 0
