@echo off
rem Clean Release build of the Windows app (all outputs rebuilt; downloaded dependency sources are kept), then packs
rem what a player needs into build\OpenLegend-<date>-<commit>.zip: openlegend.exe, the settings template
rem openlegend.toml and the MOD configurations under mods\, all at the zip root.
rem Original game data directory: OPENLEGEND_GAME_DATA_ROOT, or ..\data next to the repository.
rem Git, when found, adds the commit to the name.
rem Usage: scripts\package.bat
setlocal EnableExtensions

cd /d "%~dp0.."
set "ROOT=%CD%"
set "STAGE=%ROOT%\build\package"
set "EXE=%ROOT%\build\windows-app\src\platform\sdl3\Release\openlegend.exe"
set "DATA_DIR=%OPENLEGEND_GAME_DATA_ROOT%"
if not defined DATA_DIR set "DATA_DIR=%ROOT%\..\data"

call "%ROOT%\build.bat" app --config Release --clean --data-dir "%DATA_DIR%" || exit /b 1
if not exist "%EXE%" (
    echo %EXE% missing
    exit /b 1
)

set "DAY="
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set "DAY=%%i"
set "COMMIT="
for /f %%i in ('git -C "%ROOT%" rev-parse --short HEAD 2^>nul') do set "COMMIT=-%%i"
set "NAME=OpenLegend-%DAY%%COMMIT%"

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\mods" || exit /b 1
copy /y "%EXE%" "%STAGE%\" >nul || exit /b 1
copy /y "%ROOT%\config\openlegend.example.toml" "%STAGE%\openlegend.toml" >nul || exit /b 1
copy /y "%ROOT%\config\mods\*.toml" "%STAGE%\mods\" >nul || exit /b 1
if exist "%ROOT%\build\%NAME%.zip" del "%ROOT%\build\%NAME%.zip"
"%SystemRoot%\System32\tar.exe" -a -c -f "%ROOT%\build\%NAME%.zip" -C "%STAGE%" openlegend.exe openlegend.toml mods || exit /b 1
rmdir /s /q "%STAGE%"
echo %ROOT%\build\%NAME%.zip
