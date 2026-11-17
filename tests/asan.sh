#!/bin/sh
# tests/asan.sh -- clean ASan+UBSan build and the sanitizer test pass.
#
# Usage:
#   tests/asan.sh                  # FFI build (the default)
#   WITH_FFI=0 tests/asan.sh       # stub build: no FFI suite, no cdef fuzz
#   CC=clang tests/asan.sh         # optional compiler override
#
# The script starts with "make clean", rebuilds with ASan+UBSan in
# CFLAGS/LDFLAGS, and runs the semantics, compat, FFI (for WITH_FFI=1) and
# differential suites plus the deterministic cdef fuzzer.  It deliberately
# restores nothing: the tree is left as a sanitizer build, so run "make
# clean" (or a normal "make") before producing a non-instrumented binary.
#
# Leak detection is off because Lua states own allocations until lua_close();
# see doc/testing.md.  All sanitizer options are exported to every child.

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
WITH_FFI=${WITH_FFI:-1}

case $WITH_FFI in
  0|1) ;;
  *)
    echo "asan: WITH_FFI must be 0 or 1 (got '$WITH_FFI')" >&2
    exit 2
    ;;
esac

ASAN_CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra'
ASAN_LDFLAGS='-fsanitize=address,undefined'

ASAN_OPTIONS='detect_leaks=0:abort_on_error=1'
UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'
export ASAN_OPTIONS UBSAN_OPTIONS

# The script's own fuzzer deadline cannot interrupt a hang inside the C
# parser, so use coreutils timeout when it is available.
run_bounded () {
  _seconds=$1
  shift
  if command -v timeout >/dev/null 2>&1; then
    timeout "$_seconds" "$@"
  else
    "$@"
  fi
}

note_build_state () {
  echo "asan: note: tree left built with ASan+UBSan; run 'make clean' before a normal build"
}

trap note_build_state EXIT

cd "$ROOT"

echo "== clean =="
make clean

echo "== build: ASan+UBSan WITH_FFI=$WITH_FFI =="
# Command-line make variables propagate to src/Makefile and override its
# CFLAGS/LDFLAGS defaults.
set -- WITH_FFI="$WITH_FFI" CFLAGS="$ASAN_CFLAGS" LDFLAGS="$ASAN_LDFLAGS"
if [ -n "${CC:-}" ]; then
  set -- "$@" CC="$CC"
fi
make "$@"

echo "== semantics / compat =="
./src/luar tests/run.lua semantics compat jit

if [ "$WITH_FFI" = 1 ]; then
  echo "== ffi =="
  ./src/luar tests/run.lua ffi
fi

echo "== differential vs stock lua =="
./tests/diff/run.sh

if [ "$WITH_FFI" = 1 ]; then
  echo "== cdef fuzz (ASan+UBSan) =="
  run_bounded 120 ./src/luar tools/fuzz-cdef.lua \
    --cases "${FUZZ_CASES:-5000}" --max-seconds "${FUZZ_MAX_SECONDS:-4}"
fi

echo "asan: all checks passed"
