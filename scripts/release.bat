@echo off
rem Builds and packs the program (scripts\package.bat) and puts the zip in the GitHub release in place of the zips it
rem has: the release keeps its tag, title and notes, only its download changes.
rem Needs what package.bat needs, and the GitHub CLI (gh) logged in to the repository.
rem The release: OPENLEGEND_RELEASE (by default v0.1, the tag of the release).
rem Usage: scripts\release.bat
setlocal EnableExtensions

if not defined OPENLEGEND_RELEASE set "OPENLEGEND_RELEASE=v0.1"
set "REPO=Ouroboros/OpenLegend"
cd /d "%~dp0.."
set "ROOT=%CD%"

where gh >nul 2>nul || (
    echo gh not found ^(winget install GitHub.cli, then gh auth login^)
    exit /b 1
)

call "%~dp0package.bat" || exit /b 1

set "ZIP="
for /f "delims=" %%i in ('dir /b /o-d "%ROOT%\build\OpenLegend-*.zip"') do if not defined ZIP set "ZIP=%%i"

if not defined ZIP (
    echo no package in %ROOT%\build
    exit /b 1
)

gh release upload "%OPENLEGEND_RELEASE%" "%ROOT%\build\%ZIP%" --repo "%REPO%" --clobber || exit /b 1

rem The zips the release has under other names are taken off once the new one is up.
for /f "delims=" %%i in ('gh release view "%OPENLEGEND_RELEASE%" --repo "%REPO%" --json assets --jq ".assets[].name"') do (
    if /i "%%~xi"==".zip" if /i not "%%i"=="%ZIP%" (
        gh release delete-asset "%OPENLEGEND_RELEASE%" "%%i" --repo "%REPO%" --yes || exit /b 1
    )
)

echo %ZIP% uploaded to https://github.com/%REPO%/releases/tag/%OPENLEGEND_RELEASE%
