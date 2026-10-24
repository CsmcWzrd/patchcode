@echo off
setlocal
pushd "%~dp0.."
if not exist test-output mkdir test-output
where csc >nul 2>&1
if errorlevel 1 (
 echo Run in Developer Command Prompt for VS 2022.
 popd
 exit /b 1
)
csc /nologo /target:exe /out:test-output\CoreTests.exe /r:System.Core.dll src\Compare.cs src\NativeTool.cs tests\CoreTests.cs
if errorlevel 1 (popd & exit /b 1)
test-output\CoreTests.exe
if errorlevel 1 (popd & exit /b 1)
if not exist bin\x64\Release\tools\diffcode.exe (
 echo Native integration tests skipped: build Release x64 solution first.
 popd
 exit /b 0
)
if not exist test-output\tools mkdir test-output\tools
copy /y bin\x64\Release\tools\diffcode.exe test-output\tools\diffcode.exe >nul
csc /nologo /target:exe /out:test-output\NativeIntegrationTests.exe /r:System.Core.dll src\Compare.cs src\NativeTool.cs tests\NativeIntegrationTests.cs
if errorlevel 1 (popd & exit /b 1)
test-output\NativeIntegrationTests.exe
set result=%errorlevel%
popd
exit /b %result%
