@echo off
setlocal EnableExtensions DisableDelayedExpansion
rem Usage: build.cmd [Debug^|Release] [x64^|x86]
set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=Release"
set "BUILD_PLATFORM=%~2"
if not defined BUILD_PLATFORM set "BUILD_PLATFORM=x64"
if /I not "%BUILD_CONFIG%"=="Debug" if /I not "%BUILD_CONFIG%"=="Release" goto :usage
if /I not "%BUILD_PLATFORM%"=="x64" if /I not "%BUILD_PLATFORM%"=="x86" goto :usage
rem Normalize case for configuration-specific output paths and MSBuild properties.
if /I "%BUILD_CONFIG%"=="Debug" (set "BUILD_CONFIG=Debug") else (set "BUILD_CONFIG=Release")
if /I "%BUILD_PLATFORM%"=="x64" (set "BUILD_PLATFORM=x64") else (set "BUILD_PLATFORM=x86")
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
 echo Visual Studio Installer was not found. Install Visual Studio 2022 with .NET desktop development and Desktop development with C++.
 exit /b 1
)
for /f "usebackq delims=" %%M in (`"%VSWHERE%" -latest -products * -version "[17.0,18.0)" -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "BUILD_MSBUILD=%%M"
if not defined BUILD_MSBUILD (
 echo Visual Studio 2022 MSBuild was not found. Modify VS2022 in Visual Studio Installer to install the required workloads.
 exit /b 1
)
if not exist "%~dp0artifacts" mkdir "%~dp0artifacts"
echo Building C# GUI and included native tools: %BUILD_CONFIG%^|%BUILD_PLATFORM%
"%BUILD_MSBUILD%" "%~dp0PatchCode-GuiClient-Windows.sln" /m /t:Rebuild /p:Configuration=%BUILD_CONFIG% /p:Platform=%BUILD_PLATFORM% /v:minimal "/bl:%~dp0artifacts\build-%BUILD_CONFIG%-%BUILD_PLATFORM%.binlog"
if errorlevel 1 exit /b 1
set "GUI_OUTPUT=%~dp0bin\%BUILD_PLATFORM%\%BUILD_CONFIG%"
if not exist "%GUI_OUTPUT%\PatchCode-GuiClient-Windows.exe" (
 echo ERROR: The C# GUI executable was not produced. See artifacts for the build log.
 exit /b 1
)
if not exist "%GUI_OUTPUT%\tools\diffcode.exe" (
 echo ERROR: diffcode.exe was not copied beside the GUI.
 exit /b 1
)
if not exist "%GUI_OUTPUT%\tools\patchcode.exe" (
 echo ERROR: patchcode.exe was not copied beside the GUI.
 exit /b 1
)
echo.
echo BUILD VERIFIED: "%GUI_OUTPUT%\PatchCode-GuiClient-Windows.exe"
exit /b 0
:usage
echo Usage: build.cmd [Debug^|Release] [x64^|x86]
exit /b 2
