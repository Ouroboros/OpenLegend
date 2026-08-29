@echo off
setlocal

cd /d "%~dp0"

set "CMAKE_EXE=D:\Dev\lldb\tools\cmake\bin\cmake.exe"
set "CTEST_EXE=D:\Dev\lldb\tools\cmake\bin\ctest.exe"
set "NINJA_EXE=D:\Dev\lldb\tools\ninja\ninja.exe"
set "LLVM_BIN=D:\Dev\Compiler\LLVM\x64\bin"

if not exist "%CMAKE_EXE%" goto missing_tools
if not exist "%CTEST_EXE%" goto missing_tools
if not exist "%NINJA_EXE%" goto missing_tools
if not exist "%LLVM_BIN%\clang++.exe" goto missing_tools

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=core"
if /I not "%TARGET%"=="core" if /I not "%TARGET%"=="app" if /I not "%TARGET%"=="sdl" goto usage
if /I "%TARGET%"=="app" if not exist "%LLVM_BIN%\clang.exe" goto missing_tools
if /I "%TARGET%"=="sdl" if not exist "%LLVM_BIN%\clang.exe" goto missing_tools

set "PATH=%LLVM_BIN%;D:\Dev\lldb\tools\cmake\bin;D:\Dev\lldb\tools\ninja;%PATH%"
set "OPENLEGEND_CMAKE=%CMAKE_EXE%"
set "OPENLEGEND_CTEST=%CTEST_EXE%"
set "OPENLEGEND_NINJA=%NINJA_EXE%"
set "CC=%LLVM_BIN%\clang.exe"
set "CXX=%LLVM_BIN%\clang++.exe"

where python >nul 2>nul
if errorlevel 1 (
    if exist "D:\Dev\Python\python.exe" (
        set "PYTHON=D:\Dev\Python\python.exe"
    ) else (
        echo [OpenLegend] Python 3 was not found. 1>&2
        exit /b 2
    )
) else (
    set "PYTHON=python"
)

"%PYTHON%" "%~dp0tools\build.py" %*
set "BUILD_EXIT=%ERRORLEVEL%"
echo.
exit /b %BUILD_EXIT%

:usage
echo Usage: build.bat [core^|app] [--config Debug^|Release] [options] 1>&2
exit /b 2

:missing_tools
echo [OpenLegend] Required tool not found. 1>&2
echo CMake: %CMAKE_EXE% 1>&2
echo CTest: %CTEST_EXE% 1>&2
echo Ninja: %NINJA_EXE% 1>&2
echo LLVM C: %LLVM_BIN%\clang.exe 1>&2
echo LLVM C++: %LLVM_BIN%\clang++.exe 1>&2
exit /b 2
