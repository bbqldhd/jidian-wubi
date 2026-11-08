REM SPDX-License-Identifier: MulanPub-2.0
REM Copyright (c) 2026 Charry
REM Freeime11 is licensed under Mulan PubL v2.
REM You can use this software according to the terms and conditions of the Mulan PubL v2.
REM You may obtain a copy of Mulan PubL v2 at: http://license.coscl.org.cn/MulanPubL-2.0
REM THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
REM EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
REM MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
REM See the Mulan PubL v2 for more details.

@echo off
setlocal EnableExtensions
cd /d "%~dp0"

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

echo Installing FreeIME 11 (JiDian Wubi) v11.0.1 ...

set "INSTALLDIR=%ProgramFiles%\FreeIME11"
if not exist "%INSTALLDIR%" mkdir "%INSTALLDIR%"
copy /y "%~dp0FreeIME11.dll" "%INSTALLDIR%\" >nul
copy /y "%~dp0freeime11-migrate.exe" "%INSTALLDIR%\" >nul
copy /y "%~dp0freeime11-settings.exe" "%INSTALLDIR%\" >nul
copy /y "%~dp0freeime11-uninstall.exe" "%INSTALLDIR%\" >nul

regsvr32 /s "%INSTALLDIR%\FreeIME11.dll"

set "DATADIR=%APPDATA%\FreeIME11"
if not exist "%DATADIR%\data\mb\default" mkdir "%DATADIR%\data\mb\default"
copy /y "%~dp0data\mb\default\wbzx.mb" "%DATADIR%\data\mb\default\" >nul 2>&1
copy /y "%~dp0data\mb\default\pinyin.mb" "%DATADIR%\data\mb\default\" >nul 2>&1

echo.
echo Done. Add "FreeIME 11" in Settings - Time & Language - Language & Region.
echo To migrate from 10.8.9: run "%INSTALLDIR%\freeime11-migrate.exe" "old-install-dir"
pause
