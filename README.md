# patchcode

# Information:

Designed by Anoop Kumar Narayanan

Coded by ChatGPT 5.5 (Originally coded by Anoop Kumar Narayanan, only patch tool was present and the repo name was twopatch)

Patchcode is a simple editable text-file patching mechanism.

The package builds two command-line tools:

- `patchcode` applies patchcode patch files.
- `diffcode` generates patchcode patch files by comparing an original text file with an updated text file, or by comparing two directories.

Patch files contain metadata followed by three code-location sections:

1. start code location
2. updated code
3. end code location

Each section is separated by a line containing exactly:

```text
<@+@+@>
```

The old single `@` separator is no longer used, so ordinary `@` characters can appear in patch code without being treated as section delimiters.

## Build options

The repository now includes four build-system options:

1. Plain Makefile
2. Autoconf-generated configuration with the same Makefile
3. CMake project files
4. Visual Studio 2022 C++ solution and project files

### Build with plain Make

```sh
make
```

This creates:

```text
build/make/patchcode
build/make/diffcode
```

Useful Make targets:

```sh
make test
make install PREFIX=/usr/local
make clean
```

### Build with Autoconf

The package includes `configure.ac`, `config.mk.in`, and a generated `configure` script.

```sh
./configure --prefix=/usr/local
make
make test
make install
```

The Autoconf flow writes `config.mk`, which is included by the plain Makefile.

If `configure` needs to be regenerated:

```sh
autoconf
```

### Build with CMake

```sh
cmake -S . -B build
cmake --build build
```

This creates both executables under the build tree, normally:

```text
build/src/patchcode
build/src/diffcode
```

On Windows generators, binaries may be under a configuration directory such as:

```text
build/src/Debug
build/src/Release
```

Run tests with CMake:

```sh
ctest --test-dir build --output-on-failure
```

Install with CMake:

```sh
cmake --install build --prefix /usr/local
```

### Build with Visual Studio 2022

Open:

```text
vs2022/patchcode.sln
```

The solution contains two console application projects:

- `patchcode`
- `diffcode`

Use `Build > Build Solution` in Visual Studio 2022. The output binaries are written to:

```text
vs2022/bin/<Platform>/<Configuration>/
```

For example:

```text
vs2022/bin/x64/Release/patchcode.exe
vs2022/bin/x64/Release/diffcode.exe
```

The project files use the Visual Studio 2022 `v143` platform toolset.

## Apply a patch

```sh
patchcode path/to/file.patch
```

`patchcode` reports the result of every patch segment as one of:

- applied
- already applied
- disabled
- failed

It also prints a final summary and final status line. Reapplying the same patch is detected for normal replacement/truncate/append cases when the target replacement area already equals the patch change section.

By default, `patchcode` does not create backup files. To create one timestamped backup for each target file modified during the run, pass `--backup`:

```sh
patchcode --backup path/to/file.patch
```

The option may also be placed after the patch filename:

```sh
patchcode path/to/file.patch --backup
```

## Undo a patch

When a patch changes files, `patchcode` creates an undo patch next to the original patch file:

```text
path/to/file.patch.undo.patch
```

Undo the last successful application of that patch with:

```sh
patchcode --undo-patch path/to/file.patch
```

You may also apply the generated undo patch directly:

```sh
patchcode --undo-patch path/to/file.patch.undo.patch
```

The undo patch records the original removed/replaced text at apply time. Because the original text is not always present in a patchcode patch file, undo information is generated during patch application rather than reconstructed later from the original patch file alone.

## Generate a patch with diffcode

```sh
diffcode [options] <original-file> <updated-file> [output.patch]
diffcode [options] <original-dir>  <updated-dir>  [output.patch]
```

Single-file example:

```sh
diffcode --filename src/example.cpp \
         --author "<fill-author>" \
         --patchname "<fill-patch-name>" \
         --bugs "<fill-bug-ids>" \
         original/example.cpp updated/example.cpp example.patch
```

Directory example:

```sh
diffcode dir1 dir2 outputpatchcodepatchfile.patch
```

In directory mode, `diffcode` recursively compares regular files below `dir1` and `dir2`. The generated `filename:` values target `dir1/<relative-path>` by default, so applying the patch from the same working directory transforms the original directory tree toward the updated directory tree:

```sh
patchcode outputpatchcodepatchfile.patch
```

Use `--target-root <dir>` to write a different target root into the generated `filename:` headers.

`diffcode` emits normal patchcode segments. Each segment contains fillable metadata headers and generated pattern anchors:

```text
filename:src/example.cpp
author:<fill-author>
patchname:<fill-patch-name>
timestamp:2026/07/05-10:00:00
enable:yes
bugs:<fill-bug-ids>
extra:Generated by diffcode; review pattern anchors before applying.
<@+@+@>
start pattern lines
<@+@+@>
updated code lines
<@+@+@>
end pattern lines
<@+@+@>
```

Directory-mode behavior:

- Files present in both directories are compared line-by-line and emitted as normal patchcode replacement hunks.
- Files present only in the updated directory are emitted as full-file creation segments. `patchcode` creates missing parent directories before writing the target file.
- Files present only in the original directory are emitted as empty full-file replacement segments for review. This keeps the generated patch inside the current patchcode patch design; patchcode does not physically delete files.
- Binary files are skipped silently by default.
- Files containing a line exactly equal to `<@+@+@>` are skipped with a warning because they cannot be represented safely in the current text patch format.

`diffcode` is intentionally quiet by default:

- identical inputs produce no output and no patch file;
- text differences print only a short message saying that the files/directories differ and that a patchcode patch was created;
- binary files are skipped by default;
- use `--check-binary` to compare binary files and report binary differences. Binary differences are reported only; no binary patchcode patch is generated.

Useful `diffcode` options:

- `-o <file>` writes the patch output to a file.
- `--filename <path>` controls the `filename:` metadata that `patchcode` will modify in single-file mode.
- `--target-root <dir>` controls the generated `filename:` root in directory mode.
- `--author`, `--patchname`, `--bugs`, and `--extra` set the fillable headers.
- `--context <n>` sets the minimum number of anchor lines, default `3`.
- `--max-context <n>` lets anchors expand to reduce duplicate pattern matches, default `8`.
- `--disable` emits `enable:no` for review-only patches.
- `--check-binary` compares binary files and reports binary differences without generating binary patch segments.

`diffcode` is line based and intended for text files. Binary files are skipped by default because the current patchcode patch format is text oriented.
