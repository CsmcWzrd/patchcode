@echo off
setlocal
pushd "%~dp0.."
if not exist test-output mkdir test-output
csc /nologo /target:exe /main:GuiSmokeTests /out:test-output\GuiSmokeTests.exe /r:System.Core.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll src\*.cs tests\GuiSmokeTests.cs
if errorlevel 1 (popd & exit /b 1)
test-output\GuiSmokeTests.exe
set result=%errorlevel%
popd
exit /b %result%
