# Source review — 24 October 2026

Reviewed repository: https://github.com/CsmcWzrd/patchcode/
Pinned main commit: 8ed8e75785984420c708603aad8a6a8beef20282.
Scope: native diff generation, patch parsing/application, file I/O, CLI behavior, build projects and tests. This is an integration review, not a claim that upstream is production-qualified.

## Contract retained

`src/diffcode_main.cpp` emits seven metadata keys, followed by start anchor, replacement and end anchor sections, delimited by four exact `<@+@+@>` lines per segment. Comments belong to the metadata region. The EOF truncate marker is `----++++endoffile++++----`. Directory mode emits repeated segments. `src/patchcode_tool.cpp` recognizes `enable:yes` and `enable:true`; filename, author, patchname, timestamp and bugs must be nonempty. Unknown metadata keys are ignored upstream. The GUI edits this actual document, with no schema form or alternate serializer.

## Findings and integration decisions

| Finding | Evidence | Consequence / treatment |
|---|---|---|
| ANSI filesystem paths on Windows | `GetFileAttributesA`, `FindFirstFileA`, `main(char**)` in diffcode; narrow streams and `_mkdir` in patchcode | GUI viewing supports Unicode; export rejects non-ASCII input paths, descendant filenames and TEMP. No native source modification. |
| Link traversal in native Windows recursion | `list_files_recursive_impl` recurses on directory attribute without a reparse-point check | GUI inventories both trees first and rejects links/junctions. Concurrent changes after the check are not an atomic snapshot. |
| Non-atomic writes and incomplete error propagation | `patchcode_file::rewrite_file()` truncates in place, returns void, does not check every write; caller counts an application before rewrite | Do not infer reliable application from CLI exit code alone. No GUI Apply action is exposed in this diff client. Native tool remains included for separate use. |
| Parser errors may still yield exit 0 | `process_end()` discards invalid metadata; `read_patches()` logs incomplete segments; main can finish with no changes and return 0 | Blank template must be filled. The GUI offers raw editing, not a claim of patch validity. |
| Directory input failures can be warnings/skips | `run_directory_mode` continues after read failures and unrepresentable files | Tool log retained and warnings surfaced; successful export does not guarantee complete coverage. |
| Pattern ambiguity | `find_pattern` chooses the first match; diffcode expands anchors only up to configured maximum | Review anchors before application, particularly repetitive code. GUI previews are not an application simulator. |
| No native deletion/empty-file identity | Removed files become full empty replacement; empty old/new line vectors compare equal | Tree shows presence differences; exported result may empty a file or omit an empty-file-only change. |
| No separator escaping | `validate_text_for_patch` skips files with an exact separator line | GUI displays these files, export reports upstream limitations. No format extension added. |
| Missing final newline cannot be faithfully encoded in generated payload | `write_payload_line` inserts LF when missing | Display final-newline differences explicitly; document export limitation. |
| Binary support is report-only | `file_looks_binary`, `--check-binary` | GUI detects/reports binary changes; native export uses `--check-binary`. |
| Bounded but potentially expensive native LCS | 25,000,000-cell threshold and prefix/suffix fallback | GUI uses a smaller bounded preview matrix; native process is asynchronous and cancellable with a timeout. |
| No root confinement when applying edited metadata | filename values are directly used as targets | Raw patch text is data to review. This GUI never executes loaded patches or applies edits. |

## Integration boundaries

The GUI computes its own bounded line alignment solely for presentation. `diffcode.exe` remains the sole patch generator. Left/right alignment can choose different equally valid matching lines than native LCS; it does not claim native hunk correspondence. Preview text is not written back to source files. Native generation uses quoted arguments with `UseShellExecute=false`, redirects both streams asynchronously and writes to a fresh unique temporary output so an identical-input run cannot reuse a stale patch.

Vendored C++/header files are byte-identical to the pinned repository. Visual Studio projects retain v143 and their source lists; output paths gain a `native` subdirectory and runtime linkage is static. The top-level solution maps x86 to Win32 for native projects and declares tool dependencies before the GUI. The 25 October build correction adds explicit C# project configurations and native ProjectReference items. Native output locations are anchored to the native project files, so direct GUI-project builds do not require SolutionDir. The native-only upstream solution is retained as a .txt document to avoid opening it by mistake. Existing upstream standalone build files and tests are included.
