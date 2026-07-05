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
