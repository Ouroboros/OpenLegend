@echo off
setlocal
set "ROOT=%~dp0"
where python >nul 2>nul
if errorlevel 1 (
    if exist "D:\Dev\Python\python.exe" (
        set "PYTHON=D:\Dev\Python\python.exe"
    ) else (
        echo Python 3 was not found. 1>&2
        exit /b 1
    )
) else (
    set "PYTHON=python"
)
"%PYTHON%" "%ROOT%tools\build.py" %*
exit /b %errorlevel%
