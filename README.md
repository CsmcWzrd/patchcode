# patchcode

Patchcode is a simple editable text-file patching mechanism.

Patch files contain metadata followed by three code-location sections:

1. start code location
2. updated code
3. end code location

Each section is separated by a line containing exactly:

```text
<@+@+@>
```

The old single `@` separator is no longer used, so ordinary `@` characters can appear in patch code without being treated as section delimiters.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Run

```sh
patchcode path/to/file.patch
```
