@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-final-package.ps1"
if errorlevel 1 (
  echo.
  echo FINAL PACKAGE PREFLIGHT FAILED.
  pause
  exit /b 1
)
echo.
echo FINAL PACKAGE PREFLIGHT PASSED.
pause
