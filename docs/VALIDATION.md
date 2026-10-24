# Validation — build correction, 25 October 2026

## Tests completed for this package

| Check | Result |
|---|---|
| Actual C# project Rebuild — Debug x64 | Passed; managed PE machine 0x8664 |
| Actual C# project Rebuild — Release x64 | Passed; managed PE machine 0x8664 |
| Actual C# project Rebuild — Debug x86 | Passed; managed PE machine 0x014c |
| Actual C# project Rebuild — Release x86 | Passed; managed PE machine 0x014c |
| Output configuration, template, license and two native tool copies | Verified for all four configurations |
| Native staging with a deliberately missing diffcode executable | Failed as expected with the explicit missing-tool error |
| Build/solution configuration regression suite | 10 checks passed |
| C# core comparison, quoting, metadata and directory tests | 27 checks passed |
| C# wrapper invoking the actual native diff generator | 4 checks passed |
| Upstream native regression suite | 15 cases passed |
| Upstream native source and all five GUI .cs files | Byte-identical to the previous package |
| XML parsing, source inclusions, archive integrity and single solution entry point | Verified |

This is **56 automated functional/configuration checks**, plus four managed project builds and a negative native-staging check. The build configuration suite detects missing configuration groups, missing native project references and the extra native-only solution in the previous package.

## Exact environment and limits

Tests ran on Linux. The managed builds used Mono 6.8 xbuild 14.0 on the actual corrected .csproj, not a manually assembled list of C# files. They used the installed Mono .NET Framework 4.8 reference assemblies, an environment-local compiler wrapper and an explicit LangVersion=7.2 override because Mono's compiler does not accept VS2022's configured 7.3 value. No source changes were needed. The old xbuild toolset emits its warning that v4.8 is newer than the toolset; Windows VS2022 uses its current MSBuild toolset and .NET Framework targeting pack.

Windows VC++ project builds were disabled only for these Linux managed-build tests using BuildNativeTools=false. StageNativeTools=true exercised the real post-build copy/error target against separately g++-compiled native tools. The tests checked each managed executable's PE architecture, output config/template/license and exact staged native bytes. The x86 runs verify the C# output architecture and configuration mapping, not a 32-bit native compiler build; the staged Linux native test executable was x86_64.

The native regression runner used the upstream Makefile-built tools in place of its CMake setup; upstream test assertions and native sources were unchanged. Native wrapper integration used the real Linux executable temporarily named diffcode.exe. No Linux binaries or build outputs are included in the source ZIP.

A Windows desktop, Microsoft MSBuild and MSVC v143 are not available here. **An end-to-end VS2022 Community/Professional solution build and Windows GUI execution remain unverified.** The source/configuration fixes and available tests should not be described as a completed Windows qualification. No new GUI smoke execution is claimed.

## Reproduce on Windows

1. Extract to a fresh folder. Follow START-HERE.txt and install both required VS2022 workloads and the .NET Framework 4.8 targeting pack.
2. Run `build.cmd` from an ordinary Command Prompt. It must report the GUI executable path and verify both copied native tools. The script finds VS2022 automatically and writes `artifacts/build-Release-x64.binlog`.
3. Exercise the other configurations with `build.cmd Debug x64`, `build.cmd Release x86` and `build.cmd Debug x86`.
4. In a VS2022 Developer Command Prompt, run `tests/run-tests.cmd` for the 27 core and 4 native-wrapper checks. Run `tests/run-gui-smoke.cmd` on an interactive Windows desktop for the GUI smoke test.
5. If Python 3 is installed, `python tests/BuildConfigurationTests.py` runs the 10 structural build regressions. Python is not needed to build or use the application.
6. Manually compare files/folders, select files in all three tree tabs, switch both diff views, generate/edit/save a native patch, check unsaved prompts and verify Cancel.

Build and test logs are in `docs/test-results`. They contain only this project's compilation/test output; the expected missing-tool failure log is included as a negative test.
