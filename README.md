# PatchCode-GuiClient-Windows

Visual C# Windows Forms two-way diff client for Visual Studio **2022 Community and Professional**. Targets .NET Framework 4.8. Includes the MIT-licensed PatchCode and DiffCode C++ sources pinned to upstream commit `8ed8e75785984420c708603aad8a6a8beef20282`.

## Build — corrected package, 25 October 2026

**Extract this update to a new folder. Open the root `PatchCode-GuiClient-Windows.sln`.** The obsolete native-only `third_party/patchcode/vs2022/patchcode.sln` has been removed as a solution entry point; its original text is retained in `docs/upstream-native-solution.txt`. There is now one build solution containing the C# GUI and both native tools.


1. In Visual Studio Installer, install **.NET desktop development**, the **.NET Framework 4.8 targeting pack**, and **Desktop development with C++**, including the **MSVC v143** toolset and Windows 10/11 SDK.
2. Extract the entire ZIP into a writable directory.
3. Open `PatchCode-GuiClient-Windows.sln` at the package root. Set `PatchCode-GuiClient-Windows` as the startup project if necessary.
4. Choose **Release | x64** (or Debug / x86) and **Build → Build Solution**. No NuGet packages or external editor components are required.
5. Run `bin/x64/Release/PatchCode-GuiClient-Windows.exe`, or press F5.

`build.cmd` builds Release x64 from any Command Prompt, locating Visual Studio 2022 MSBuild automatically. Use `build.cmd Debug x64` or `build.cmd Release x86` for other configurations. The script rebuilds the complete solution, saves a binary build log in `artifacts`, and fails unless the GUI executable and both copied native tools exist. The solution builds both native tools before the C# client and copies them into the GUI output's `tools` folder. The C# project now contains actual native project references, so building that project directly with MSBuild also builds its tools. The four explicit Debug/Release × x64/x86 C# configurations match the solution mappings, with x86 mapped to Win32 for C++. Deploy the entire output directory, including `tools` and its license. The .NET Framework 4.8 runtime must be installed on the destination Windows machine. Native projects use the static C++ runtime.

## Use

- **File → Compare two files / Compare two folders** selects the old and new inputs. Folder mode scans recursively, pairing paths case-insensitively as on Windows. Added, removed, modified, identical and binary files are distinguished. Empty directories have no diff entries.
- The left pane contains **Folder 1 (old code)**, **Folder 2 (new code)** and **Diff** tree tabs. Selecting a file in a folder tree opens its read-only source. Selecting an entry in Diff opens a dedicated comparison tab. Repeated selection reuses the tab.
- **View → Left right comparison** shows aligned old/new lines with a shared vertical scroll position and line numbers. Missing sides are blank. **Merged view** shows context plus deletions with a **Red** background and additions with a **Blue** background. This view is a two-way presentation, not a three-way merge or source editor.
- F7 / Shift+F7 navigates changed blocks. Ctrl+W or the toolbar closes a tab. Ctrl+O selects files; Ctrl+Shift+O selects folders; F5 rescans, closing old previews after prompting for unsaved patches.
- **File → Generate native patch** runs the included `diffcode.exe` on the current inputs. It does not use the GUI alignment algorithm to serialize a patch. Every native metadata field and all patch sections appear in one editable text tab. Multiple segments remain in the same document. Edit `filename`, `author`, `patchname`, `timestamp`, `enable`, `bugs`, and `extra` directly there. No metadata form is used.
- **File → New empty patch template** supplies the seven keys with **no values**, plus all four separators. The same file is in `templates/empty.patch`. Fill required metadata before using the patch; the empty template is intentionally not executable metadata. Set `enable:yes` or `enable:no` explicitly.
- Ctrl+S / Ctrl+Shift+S saves the active patch document. File → Open patch / metadata reopens an existing UTF-8 patch. Standard text selection, undo, cut/copy/paste and select-all are available. Source previews are read-only. Closing, choosing another comparison, refreshing and exiting prompt for unsaved patches.
- **View → Tool log** shows native output, skipped files and warnings. Binary-only changes or identical inputs can produce no patch. Warnings are displayed in their own tab after export.
- Cancel stops scanning/alignment or kills a running native export. Native export also has a two-minute timeout. Comparison and patch generation never apply changes to the input files. The included `patchcode.exe` is available for separate command-line use; this version has no GUI Apply action.

## Format and encoding

The upstream `<@+@+@>` format and native source files are unchanged. Patch metadata is not converted to JSON, unified diff or another format. Unedited patch saves retain their loaded text; normal-sized text edits retain original line endings on aligned unchanged lines. Newly edited lines use LF. Large edits may use the documented coarse alignment and normalize changed spans. Files save as UTF-8 without adding a BOM; an existing U+FEFF character is retained. Use UTF-8 source content when exporting through the text editor; invalid UTF-8 native output is rejected rather than silently converted.

Source previews decode strict UTF-8, falling back to the current Windows code page. UTF-16/NUL-containing files are reported as binary under the upstream convention. `[CRLF]`, `[LF]` and `[no newline]` annotations distinguish physical line endings in diff views; they are display labels and never written into patches. The GUI compares byte identity before rendering, so encoding-only changes can appear in the tree even when decoded previews look alike.

The native Windows tools use ANSI APIs. The GUI permits Unicode paths for viewing, but **native export requires ASCII paths, all descendant filenames and TEMP** to avoid corrupting target metadata. Copy inputs to ASCII paths for export. Links/junctions are rejected before scanning/export. File previews and patch opening are limited to 8 MiB, with a 200,000 physical-line limit in comparison. GUI alignment bounds its LCS matrix to four million cells; larger changed regions are shown as a coarse replacement. Export uses upstream's own algorithm and limits. Native generation reads current files again; refresh after external changes.

Upstream cannot represent a literal separator line, patch binaries, physically delete files, reliably preserve missing final newlines in generated payloads, or create/delete an empty file through a content-only change. Review the tool log. **Removed** in the tree describes the input trees; an exported removal empties the target file. Review anchors and target filenames before applying patches separately. See `docs/REVIEW.md`.

## Validation

`tests/run-tests.cmd` compiles and runs core comparison/quoting/metadata-preservation tests from a VS 2022 Developer Command Prompt. See `docs/BUILD-FIX.md` for the build changes and `docs/VALIDATION.md` for the checks actually performed and remaining Windows validation. The package contains source, not a prebuilt Windows release.

## Attribution

PatchCode upstream: https://github.com/CsmcWzrd/patchcode/ . Original notices are in `third_party/patchcode/LICENSE` and its README. `docs/UPSTREAM-MANIFEST.sha256` records unchanged upstream source hashes. Only the vendored Visual Studio project output/runtime settings and solution packaging were adjusted for integration; no patch syntax or native algorithm changed. GUI source is provided under the MIT license in the root `LICENSE`.
