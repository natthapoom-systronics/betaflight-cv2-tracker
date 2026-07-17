@echo off
setlocal

REM ============================================================================
REM build_speedybee_f405v5_wsl.bat
REM One-click Windows launcher for WSL build script
REM ============================================================================

pushd "%~dp0" >nul
set "REPO_WIN=%CD%"
set "REPO_WSL="
for /f "usebackq delims=" %%i in (`wsl wslpath "%REPO_WIN%"`) do set "REPO_WSL=%%i"

where wsl.exe >nul 2>&1
if errorlevel 1 (
  echo [ERROR] WSL is not installed or not in PATH.
  exit /b 1
)

echo ==========================================
echo  Betaflight CV2 Tracker - WSL Build
echo  Repo: %REPO_WIN%
echo ==========================================

if "%REPO_WSL%"=="" (
  echo [ERROR] Failed to convert Windows path to WSL path.
  popd >nul
  exit /b 1
)

REM Convert Windows path to WSL path and run script
wsl bash -lc "set -e; cd \"%REPO_WSL%\"; chmod +x ./build_speedybee_f405v5.sh; ./build_speedybee_f405v5.sh"
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" (
  echo.
  echo [ERROR] Build failed with exit code %RC%.
  echo Check log: %REPO_WIN%\build_SPEEDYBEEF405V5.log
  popd >nul
  exit /b %RC%
)

echo.
echo [OK] Build complete.
echo Output folder: %REPO_WIN%\build
popd >nul
exit /b 0
