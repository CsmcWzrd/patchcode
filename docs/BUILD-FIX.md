# C# solution build correction — 25 October 2026

## What changed

The previous archive exposed two solution files: the top-level GUI solution and the upstream native-only `third_party/patchcode/vs2022/patchcode.sln`. The latter contained only `patchcode` and `diffcode`; opening it would not build the C# application. The new package exposes only the root `PatchCode-GuiClient-Windows.sln`. The original native-only solution is retained as `docs/upstream-native-solution.txt` for reference.

The previous C# project declared configuration-only Debug/Release property groups, rather than explicit Configuration|Platform groups for the solution's four configurations. It also relied on solution-only dependencies and a post-build copy from `SolutionDir`-dependent native outputs. Directly building the C# project did not build its tools. No user build log was supplied, so the exact local failure cannot be established; these are the concrete packaging/configuration issues corrected.

The corrected C# project contains:

- Explicit Debug|x64, Release|x64, Debug|x86 and Release|x86 groups, each with a PlatformTarget, output path and isolated intermediate directory.
- Standard Microsoft.Common.props and Microsoft.CSharp.targets imports, an explicit startup object and .NET Framework 4.8 targeting.
- Real ProjectReference entries for both C++ tools, with ReferenceOutputAssembly=false and x86 → Win32 platform mapping. Native executables are process dependencies, not managed assembly references.
- Native output paths anchored to their project directory rather than SolutionDir, so the full GUI project can also be built directly with VS2022 MSBuild.
- A post-build staging target that fails if either native executable is missing, rather than reporting a complete application when tools are absent.

`build.cmd` now locates VS2022 MSBuild with vswhere, rebuilds the root GUI solution, writes a binary build log, and verifies the C# executable plus both staged tools before reporting success. It works from an ordinary Command Prompt. Supported calls:

```
build.cmd
build.cmd Debug x64
build.cmd Release x86
```

.NET Framework 4.8, the user interface, C# source behavior, native source code and PatchCode's diff format are unchanged.

## Use the new package

Extract into a new folder, not on top of the previous archive. Open the root `PatchCode-GuiClient-Windows.sln`, choose Release|x64 and Build Solution. Or run `build.cmd`. The default executable is:

```
bin\x64\Release\PatchCode-GuiClient-Windows.exe
```

Install both `.NET desktop development` and `Desktop development with C++`, the .NET Framework 4.8 targeting pack, MSVC v143 and a Windows SDK in VS2022 Community or Professional. If a build fails, the script returns an error and the binary log is in `artifacts`.

Advanced validation switches in the project are BuildNativeTools and StageNativeTools. Both default to true for normal builds. Linux validation explicitly disabled Windows-native project builds and enabled staging of separately compiled test tools. These switches do not replace MSVC when creating a Windows deployment.
