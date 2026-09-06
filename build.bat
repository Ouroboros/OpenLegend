@echo off
chcp 65001 >nul
setlocal
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"

for %%I in ("%~dp0.") do set "PROJECT_ROOT=%%~fI"
cd /d "%PROJECT_ROOT%"

set "LLVM_BIN=D:\Dev\Compiler\LLVM\x64\bin"
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
    set "PYTHON=python"
)

"%PYTHON%" "%PROJECT_ROOT%\build.py" %*
exit /b %ERRORLEVEL%
