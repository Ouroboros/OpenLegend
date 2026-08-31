@echo off
setlocal
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

for %%I in ("%~dp0.") do set "PROJECT_ROOT=%%~fI"
if not defined PROJECT_ROOT (
    echo [OpenLegend] Failed to resolve the Windows project path. 1>&2
    exit /b 2
)
cd /d "%PROJECT_ROOT%"

set "LLVM_BIN=D:\Dev\Compiler\LLVM\x64\bin"

if not exist "%LLVM_BIN%\clang++.exe" goto missing_tools

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=core"
if /I not "%TARGET%"=="core" if /I not "%TARGET%"=="app" if /I not "%TARGET%"=="sdl" goto usage
if /I "%TARGET%"=="app" if not exist "%LLVM_BIN%\clang.exe" goto missing_tools
if /I "%TARGET%"=="sdl" if not exist "%LLVM_BIN%\clang.exe" goto missing_tools

set "PATH=%LLVM_BIN%;%PATH%"
set "OPENLEGEND_PROJECT_ROOT=%PROJECT_ROOT%"
set "OPENLEGEND_CMAKE="
set "OPENLEGEND_CTEST="
set "OPENLEGEND_NINJA="
set "CC=%LLVM_BIN%\clang.exe"
set "CXX=%LLVM_BIN%\clang++.exe"

if exist "D:\Dev\Python\python.exe" (
    set "PYTHON=D:\Dev\Python\python.exe"
) else (
    where python >nul 2>nul
    if errorlevel 1 (
        echo [OpenLegend] Python 3 was not found. 1>&2
        exit /b 2
    )
    set "PYTHON=python"
)

"%PYTHON%" "%PROJECT_ROOT%\tools\build.py" %*
set "BUILD_EXIT=%ERRORLEVEL%"
echo.
exit /b %BUILD_EXIT%

:usage
echo Usage: build.bat [core^|app] [--config Debug^|Release] [options] 1>&2
exit /b 2

:missing_tools
echo [OpenLegend] Required compiler not found. 1>&2
echo LLVM C: %LLVM_BIN%\clang.exe 1>&2
echo LLVM C++: %LLVM_BIN%\clang++.exe 1>&2
exit /b 2
