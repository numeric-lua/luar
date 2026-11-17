#!/bin/sh
# tests/diff/run.sh -- run the same scripts under stock Lua 5.5 and LuaR and
# compare stdout and exit status.  This is the anti-fork guard.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
LUAR="$ROOT/src/luar"
LUA=${LUA:-lua}
TMP="$ROOT/tests/tmp/diff"
mkdir -p "$TMP"

if ! command -v "$LUA" >/dev/null 2>&1; then
  echo "diff: skip (no stock 'lua' on PATH)"
  exit 0
fi

failed=0
total=0
for f in "$ROOT"/tests/diff/cases/*.lua; do
  name=$(basename "$f" .lua)
  total=$((total + 1))
  set +e
  "$LUA" "$f" >"$TMP/$name.lua.out" 2>"$TMP/$name.lua.err"
  lua_rc=$?
  "$LUAR" "$f" >"$TMP/$name.luar.out" 2>"$TMP/$name.luar.err"
  luar_rc=$?
  set -e
  if [ "$lua_rc" -ne "$luar_rc" ]; then
    echo "diff: FAIL $name: exit status $lua_rc vs $luar_rc" >&2
    failed=$((failed + 1))
  elif ! cmp -s "$TMP/$name.lua.out" "$TMP/$name.luar.out"; then
    echo "diff: FAIL $name: stdout differs" >&2
    diff -u "$TMP/$name.lua.out" "$TMP/$name.luar.out" | head -20 >&2 || true
    failed=$((failed + 1))
  fi
done

if [ "$failed" -ne 0 ]; then
  echo "diff: $failed/$total case(s) differ from stock Lua" >&2
  exit 1
fi
echo "diff: $total case(s) identical to stock Lua"
