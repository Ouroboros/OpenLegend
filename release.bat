@echo off
rem Clean Release build (all outputs rebuilt) of the Windows app, then pack a distributable zip into dist\.
rem Usage: release.bat [VERSION]
rem   VERSION defaults to "git describe --tags --always --dirty".
rem   Original game data directory: OPENLEGEND_GAME_DATA_ROOT, or ..\data next to the repository.
setlocal
for %%I in ("%~dp0.") do set "PROJECT_ROOT=%%~fI"
cd /d "%PROJECT_ROOT%"

set "VERSION=%~1"
if "%VERSION%"=="" (
    for /f "delims=" %%V in ('git describe --tags --always --dirty') do set "VERSION=%%V"
)
if "%VERSION%"=="" (
    echo [OpenLegend] Unable to determine the release version.
    exit /b 1
)
set "DATA_DIR=%OPENLEGEND_GAME_DATA_ROOT%"
if "%DATA_DIR%"=="" set "DATA_DIR=%PROJECT_ROOT%\..\data"

set "BUILD_DIR=%PROJECT_ROOT%\build\windows-app"
set "EXE=%BUILD_DIR%\src\platform\sdl3\Release\openlegend.exe"
set "PACKAGE=OpenLegend-%VERSION%-windows-x64"
set "DIST_DIR=%PROJECT_ROOT%\dist"
set "STAGE=%DIST_DIR%\%PACKAGE%"
set "ZIP=%DIST_DIR%\%PACKAGE%.zip"

echo [OpenLegend] Release %VERSION%
rem --clean removes all build outputs first; downloaded dependency sources are kept.
call "%PROJECT_ROOT%\build.bat" app --config Release --clean --data-dir "%DATA_DIR%"
if errorlevel 1 (
    echo [OpenLegend] Build failed.
    exit /b 1
)
if not exist "%EXE%" (
    echo [OpenLegend] Missing %EXE%
    exit /b 1
)

if exist "%STAGE%" rmdir /s /q "%STAGE%"
if exist "%ZIP%" del /q "%ZIP%"
mkdir "%STAGE%" || exit /b 1
copy /y "%EXE%" "%STAGE%\openlegend.exe" >nul || exit /b 1
copy /y "%PROJECT_ROOT%\config\openlegend.example.toml" "%STAGE%\openlegend.toml" >nul || exit /b 1
copy /y "%PROJECT_ROOT%\config\battle_merges.toml" "%STAGE%\battle_merges.toml" >nul || exit /b 1

rem tar.exe writes zip entries with standard forward-slash paths; files sit at the zip root.
"%SystemRoot%\System32\tar.exe" -a -c -f "%ZIP%" -C "%STAGE%" openlegend.exe openlegend.toml battle_merges.toml
if errorlevel 1 (
    echo [OpenLegend] Packing failed.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference='Stop';" ^
    "$hash=(Get-FileHash -Algorithm SHA256 '%ZIP%').Hash.ToLower();" ^
    "Set-Content -Encoding ascii -Path '%ZIP%.sha256' -Value ($hash + '  %PACKAGE%.zip')"
if errorlevel 1 (
    echo [OpenLegend] Checksum failed.
    exit /b 1
)
rmdir /s /q "%STAGE%"

echo [OpenLegend] Package: %ZIP%
type "%ZIP%.sha256"
exit /b 0
