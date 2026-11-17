#!/bin/sh
# Build tests/ffi/helper.c into tests/tmp/libluar_ffi_helper.so.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
src="$root/tests/ffi/helper.c"
out="$root/tests/tmp/libluar_ffi_helper.so"

mkdir -p "$root/tests/tmp"
if [ -f "$out" ] && [ "$out" -nt "$src" ]; then
  exit 0
fi

: "${CC:=cc}"
"$CC" -O2 -fPIC -shared -o "$out" "$src"
