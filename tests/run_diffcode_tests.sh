#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build"
WORK_DIR="${TMPDIR:-/tmp}/patchcode_diffcode_tests_$$"
mkdir -p "$WORK_DIR"
trap 'rm -rf "$WORK_DIR"' EXIT INT TERM

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" >/dev/null

PATCHCODE="$BUILD_DIR/src/patchcode"
DIFFCODE="$BUILD_DIR/src/diffcode"
if [ ! -x "$PATCHCODE" ]; then PATCHCODE="$BUILD_DIR/src/Debug/patchcode.exe"; fi
if [ ! -x "$DIFFCODE" ]; then DIFFCODE="$BUILD_DIR/src/Debug/diffcode.exe"; fi

run_case() {
    name="$1"
    original_text="$2"
    updated_text="$3"
    printf "%s" "$original_text" > "$WORK_DIR/${name}_original.txt"
    printf "%s" "$updated_text" > "$WORK_DIR/${name}_updated.txt"
    cp "$WORK_DIR/${name}_original.txt" "$WORK_DIR/${name}_work.txt"

    "$DIFFCODE" --filename "$WORK_DIR/${name}_work.txt" \
        --author Tester --patchname "$name" --bugs BUG \
        "$WORK_DIR/${name}_original.txt" "$WORK_DIR/${name}_updated.txt" \
        "$WORK_DIR/${name}.patch" >/dev/null

    "$PATCHCODE" "$WORK_DIR/${name}.patch" >/dev/null
    cmp "$WORK_DIR/${name}_work.txt" "$WORK_DIR/${name}_updated.txt" >/dev/null
    echo "PASS $name"
}

run_case middle 'line 1
line 2
line 3
line 4
line 5
line 6
line 7
line 8
line 9
line 10
line 11
line 12
' 'line 1
line 2
line 3
line 4 changed
line 5 changed
line 6
line 7
inserted A
inserted B
line 8
line 9
line 10
line 11
line 12
'

run_case beginning 'a
b
c
d
e
f
g
h
' 'A
B
c
d
e
f
g
h
'

run_case end 'a
b
c
d
e
f
g
h
' 'a
b
c
d
e
f
G
H
I
'

run_case small_full 'one
two
' 'three
four
five
'

run_case insert_only 'one
two
three
four
five
' 'one
two
INSERT
three
four
five
'

echo "All diffcode generation tests passed."

# Backup behavior: patchcode should not create backups unless --backup is supplied.
BACKUPCASE="$WORK_DIR/backupcase"
mkdir -p "$BACKUPCASE"
printf 'one\ntwo\nthree\n' > "$BACKUPCASE/original.txt"
printf 'one\nTWO changed\nthree\n' > "$BACKUPCASE/updated.txt"
cp "$BACKUPCASE/original.txt" "$BACKUPCASE/work_no_backup.txt"
"$DIFFCODE" --filename "$BACKUPCASE/work_no_backup.txt" \
    --author Tester --patchname no_backup --bugs BUG \
    "$BACKUPCASE/original.txt" "$BACKUPCASE/updated.txt" \
    "$BACKUPCASE/no_backup.patch" >/dev/null
"$PATCHCODE" "$BACKUPCASE/no_backup.patch" >/dev/null
cmp "$BACKUPCASE/work_no_backup.txt" "$BACKUPCASE/updated.txt" >/dev/null
if find "$BACKUPCASE" -maxdepth 1 -name 'work_no_backup.txt_bkp_*' | grep . >/dev/null; then
    echo "FAIL backup created without --backup" >&2
    exit 1
fi
echo "PASS no_backup_by_default"

cp "$BACKUPCASE/original.txt" "$BACKUPCASE/work_with_backup.txt"
"$DIFFCODE" --filename "$BACKUPCASE/work_with_backup.txt" \
    --author Tester --patchname with_backup --bugs BUG \
    "$BACKUPCASE/original.txt" "$BACKUPCASE/updated.txt" \
    "$BACKUPCASE/with_backup.patch" >/dev/null
"$PATCHCODE" --backup "$BACKUPCASE/with_backup.patch" >/dev/null
cmp "$BACKUPCASE/work_with_backup.txt" "$BACKUPCASE/updated.txt" >/dev/null
if ! find "$BACKUPCASE" -maxdepth 1 -name 'work_with_backup.txt_bkp_*' | grep . >/dev/null; then
    echo "FAIL backup not created with --backup" >&2
    exit 1
fi
echo "PASS backup_option"


# Directory compare mode: changed file, nested added file, and deleted file review segment.
DIRCASE="$WORK_DIR/dircase"
mkdir -p "$DIRCASE/orig/sub" "$DIRCASE/new/sub" "$DIRCASE/work/sub"
printf 'alpha\nbeta\ngamma\n' > "$DIRCASE/orig/main.txt"
printf 'keep\n' > "$DIRCASE/orig/sub/keep.txt"
printf 'remove me\n' > "$DIRCASE/orig/oldonly.txt"
cp -R "$DIRCASE/orig/." "$DIRCASE/work/"
printf 'alpha\nBETA changed\ngamma\ndelta\n' > "$DIRCASE/new/main.txt"
printf 'keep\n' > "$DIRCASE/new/sub/keep.txt"
mkdir -p "$DIRCASE/new/sub/newdir"
printf 'created\nfile\n' > "$DIRCASE/new/sub/newdir/added.txt"

"$DIFFCODE" --target-root "$DIRCASE/work" \
    --author Tester --patchname dircase --bugs BUG \
    "$DIRCASE/orig" "$DIRCASE/new" "$DIRCASE/dircase.patch" >/dev/null

"$PATCHCODE" "$DIRCASE/dircase.patch" >/dev/null
cmp "$DIRCASE/work/main.txt" "$DIRCASE/new/main.txt" >/dev/null
cmp "$DIRCASE/work/sub/keep.txt" "$DIRCASE/new/sub/keep.txt" >/dev/null
cmp "$DIRCASE/work/sub/newdir/added.txt" "$DIRCASE/new/sub/newdir/added.txt" >/dev/null
[ -f "$DIRCASE/work/oldonly.txt" ]
[ ! -s "$DIRCASE/work/oldonly.txt" ]
echo "PASS directory_mode"

# The default directory mode should write original-dir-relative target filenames.
"$DIFFCODE" "$DIRCASE/orig" "$DIRCASE/new" "$DIRCASE/default-dircase.patch" >/dev/null
grep "^filename:$DIRCASE/orig/main.txt$" "$DIRCASE/default-dircase.patch" >/dev/null
echo "PASS directory_mode_default_target_root"

# diffcode should be quiet and should not create an output patch when inputs are identical.
QUIETCASE="$WORK_DIR/quietcase"
mkdir -p "$QUIETCASE"
printf 'same\ntext\n' > "$QUIETCASE/a.txt"
printf 'same\ntext\n' > "$QUIETCASE/b.txt"
"$DIFFCODE" "$QUIETCASE/a.txt" "$QUIETCASE/b.txt" "$QUIETCASE/nochange.patch" >"$QUIETCASE/out.txt" 2>"$QUIETCASE/err.txt"
[ ! -s "$QUIETCASE/out.txt" ]
[ ! -s "$QUIETCASE/err.txt" ]
[ ! -e "$QUIETCASE/nochange.patch" ]
echo "PASS quiet_identical_no_patch"

# Binary files are skipped by default and therefore do not create patchcode patches.
BINCASE="$WORK_DIR/bincase"
mkdir -p "$BINCASE"
printf 'abc\000def\n' > "$BINCASE/bin_original.dat"
printf 'abc\000XYZ\n' > "$BINCASE/bin_updated.dat"
"$DIFFCODE" "$BINCASE/bin_original.dat" "$BINCASE/bin_updated.dat" "$BINCASE/bin.patch" >"$BINCASE/default.out" 2>"$BINCASE/default.err"
[ ! -s "$BINCASE/default.out" ]
[ ! -s "$BINCASE/default.err" ]
[ ! -e "$BINCASE/bin.patch" ]
echo "PASS binary_skipped_by_default"

# --check-binary reports binary differences, but still does not create a text patch for them.
"$DIFFCODE" --check-binary "$BINCASE/bin_original.dat" "$BINCASE/bin_updated.dat" "$BINCASE/bin.patch" >"$BINCASE/check.out" 2>"$BINCASE/check.err"
grep 'binary files differ' "$BINCASE/check.out" >/dev/null
[ ! -e "$BINCASE/bin.patch" ]
echo "PASS check_binary_reports_difference"

# Directory mode also skips binary files by default and checks them only with --check-binary.
BINDIR="$WORK_DIR/bindir"
mkdir -p "$BINDIR/orig" "$BINDIR/new"
printf 'same\n' > "$BINDIR/orig/text.txt"
printf 'same\n' > "$BINDIR/new/text.txt"
printf 'one\000two' > "$BINDIR/orig/data.bin"
printf 'one\000THREE' > "$BINDIR/new/data.bin"
"$DIFFCODE" "$BINDIR/orig" "$BINDIR/new" "$BINDIR/default.patch" >"$BINDIR/default.out" 2>"$BINDIR/default.err"
[ ! -s "$BINDIR/default.out" ]
[ ! -s "$BINDIR/default.err" ]
[ ! -e "$BINDIR/default.patch" ]
"$DIFFCODE" --check-binary "$BINDIR/orig" "$BINDIR/new" "$BINDIR/check.patch" >"$BINDIR/check.out" 2>"$BINDIR/check.err"
grep 'binary file differs: data.bin' "$BINDIR/check.out" >/dev/null
grep '1 binary file differs' "$BINDIR/check.out" >/dev/null
[ ! -e "$BINDIR/check.patch" ]
echo "PASS directory_binary_check_option"

# patchcode should generate an undo patch and --undo-patch should restore the original file.
UNDOCASE="$WORK_DIR/undocase"
mkdir -p "$UNDOCASE"
printf 'one\ntwo\nthree\nfour\nfive\n' > "$UNDOCASE/original.txt"
printf 'one\nTWO changed\nthree\ninserted\nfour\nfive\n' > "$UNDOCASE/updated.txt"
cp "$UNDOCASE/original.txt" "$UNDOCASE/work.txt"
"$DIFFCODE" --filename "$UNDOCASE/work.txt" \
    --author Tester --patchname undo_case --bugs BUG \
    "$UNDOCASE/original.txt" "$UNDOCASE/updated.txt" \
    "$UNDOCASE/change.patch" >/dev/null
"$PATCHCODE" "$UNDOCASE/change.patch" >"$UNDOCASE/apply.out" 2>"$UNDOCASE/apply.err"
cmp "$UNDOCASE/work.txt" "$UNDOCASE/updated.txt" >/dev/null
[ -s "$UNDOCASE/change.patch.undo.patch" ]
grep 'Status: patch applied' "$UNDOCASE/apply.err" >/dev/null
"$PATCHCODE" --undo-patch "$UNDOCASE/change.patch" >"$UNDOCASE/undo.out" 2>"$UNDOCASE/undo.err"
cmp "$UNDOCASE/work.txt" "$UNDOCASE/original.txt" >/dev/null
grep 'Status: undo patch applied' "$UNDOCASE/undo.err" >/dev/null
echo "PASS undo_patch_option"

# Reapplying an already applied patch should be reported without rewriting the file.
cp "$UNDOCASE/original.txt" "$UNDOCASE/work_already.txt"
"$DIFFCODE" --filename "$UNDOCASE/work_already.txt" \
    --author Tester --patchname already_case --bugs BUG \
    "$UNDOCASE/original.txt" "$UNDOCASE/updated.txt" \
    "$UNDOCASE/already.patch" >/dev/null
"$PATCHCODE" "$UNDOCASE/already.patch" >/dev/null 2>/dev/null
"$PATCHCODE" "$UNDOCASE/already.patch" >"$UNDOCASE/already.out" 2>"$UNDOCASE/already.err"
cmp "$UNDOCASE/work_already.txt" "$UNDOCASE/updated.txt" >/dev/null
grep 'Status: patch already applied' "$UNDOCASE/already.err" >/dev/null
echo "PASS already_applied_status"
