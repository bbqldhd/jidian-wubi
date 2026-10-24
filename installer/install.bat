@echo off
setlocal EnableExtensions
cd /d "%~dp0"

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo Installing FreeIME 11 (JiDian Wubi) v11.0.0 ...

set "INSTALLDIR=%ProgramFiles%\FreeIME11"
if not exist "%INSTALLDIR%" mkdir "%INSTALLDIR%"
copy /y "%~dp0FreeIME11.dll" "%INSTALLDIR%\" >nul
copy /y "%~dp0freeime11-migrate.exe" "%INSTALLDIR%\" >nul
copy /y "%~dp0freeime11-settings.exe" "%INSTALLDIR%\" >nul

regsvr32 /s "%INSTALLDIR%\FreeIME11.dll"

set "DATADIR=%APPDATA%\FreeIME11"
if not exist "%DATADIR%\data\mb\default" mkdir "%DATADIR%\data\mb\default"
copy /y "%~dp0data\mb\default\wbzx.mb" "%DATADIR%\data\mb\default\" >nul 2>&1
copy /y "%~dp0data\mb\default\pinyin.mb" "%DATADIR%\data\mb\default\" >nul 2>&1

echo.
echo Done. Add "FreeIME 11" in Settings - Time & Language - Language & Region.
echo To migrate from 10.8.9: run "%INSTALLDIR%\freeime11-migrate.exe" "old-install-dir"
pause
