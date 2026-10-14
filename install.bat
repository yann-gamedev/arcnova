@echo off
setlocal EnableExtensions
cd /d "%~dp0" || exit /b 1

if not exist "ArcNova.exe" (
    echo [ArcNova] ArcNova.exe not found. Run build.bat first.
    exit /b 1
)
if not defined LOCALAPPDATA (echo [ArcNova] LOCALAPPDATA is not defined.& exit /b 1)
if not defined APPDATA (echo [ArcNova] APPDATA is not defined.& exit /b 1)

set "ARCNOVA_INSTALL_DIR=%LOCALAPPDATA%\Programs\ArcNova"
set "ARCNOVA_START_MENU=%APPDATA%\Microsoft\Windows\Start Menu\Programs"

if not exist "%ARCNOVA_INSTALL_DIR%" mkdir "%ARCNOVA_INSTALL_DIR%" || exit /b 1
if not exist "%ARCNOVA_START_MENU%" mkdir "%ARCNOVA_START_MENU%" || exit /b 1

copy /Y "ArcNova.exe" "%ARCNOVA_INSTALL_DIR%\ArcNova.exe" >nul || (
    echo [ArcNova] Copy failed. Close running ArcNova windows and try again.
    exit /b 1
)

powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$target=Join-Path $env:ARCNOVA_INSTALL_DIR 'ArcNova.exe';" ^
  "$link=Join-Path $env:ARCNOVA_START_MENU 'ArcNova.lnk';" ^
  "$shell=New-Object -ComObject WScript.Shell;" ^
  "$shortcut=$shell.CreateShortcut($link);" ^
  "$shortcut.TargetPath=$target;" ^
  "$shortcut.WorkingDirectory=$env:USERPROFILE;" ^
  "$shortcut.IconLocation=($target + ',0');" ^
  "$shortcut.Description='ArcNova Shell';" ^
  "$shortcut.Save()" || (
    echo [ArcNova] Failed to create Start Menu shortcut.
    exit /b 1
)

echo [ArcNova] Installed to "%ARCNOVA_INSTALL_DIR%".
echo [ArcNova] Start Menu shortcut: "%ARCNOVA_START_MENU%\ArcNova.lnk"
echo [ArcNova] Press Windows, type ArcNova, and press Enter.
exit /b 0
