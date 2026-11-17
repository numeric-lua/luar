#!/bin/sh
# tests/cli.sh -- command line and ABI surface checks.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
LUAR="$ROOT/src/luar"
LUARC="$ROOT/src/luarc"
TMP="$ROOT/tests/tmp"
mkdir -p "$TMP"

fail() { echo "cli: FAIL: $*" >&2; exit 1; }
ok() { echo "cli: ok: $*"; }

# -- version ---------------------------------------------------------------
v=$("$LUAR" -v)
case "$v" in
  "LuaR "*) ;;
  *) fail "unexpected -v output: $v" ;;
esac
case "$v" in
  *"Lua 5.5.1"*) ;;
  *) fail "-v does not name the Lua base: $v" ;;
esac
ok "luar -v"

v=$("$LUARC" -v)
case "$v" in
  "LuaR "*) ;;
  *) fail "unexpected luarc -v output: $v" ;;
esac
ok "luarc -v"

# -- -e and exit codes -----------------------------------------------------
out=$("$LUAR" -e 'print(1 + 2)')
[ "$out" = "3" ] || fail "-e produced '$out'"
set +e
"$LUAR" -e 'os.exit(3)'
rc=$?
set -e
[ "$rc" -eq 3 ] || fail "os.exit(3) exit code was $rc"
ok "-e / exit codes"

# -- -j control ------------------------------------------------------------
out=$("$LUAR" -jv)
case "$out" in "LuaR "*) ;; *) fail "-jv output: $out" ;; esac
"$LUAR" -joff -e 'assert(jit.status() == false)' || fail "-joff"
"$LUAR" -j opt=hotloop=10 -e 'assert(type(jit.status()) == "boolean")' || fail "-j opt="
ok "-j control options"

# -- script arguments ------------------------------------------------------
cat > "$TMP/args.lua" <<'EOF'
print(table.concat(arg, ","))
print(arg[0] and "has0" or "no0")
EOF
out=$("$LUAR" "$TMP/args.lua" a b c)
[ "$out" = "a,b,c
has0" ] || fail "arg table wrong: $out"
ok "script arguments"

# -- bytecode roundtrip ----------------------------------------------------
cat > "$TMP/prog.lua" <<'EOF'
return 6 * 7
EOF
"$LUARC" -o "$TMP/prog.luac" "$TMP/prog.lua"
out=$("$LUAR" -e "print(dofile('$TMP/prog.luac'))")
[ "$out" = "42" ] || fail "bytecode roundtrip: $out"
ok "luarc bytecode"

# Shared source for the bytecode checks below.
cat > "$TMP/cross.lua" <<'LUAEOF'
local t = {}
for i = 1, 5 do t[i] = i * i end
print(table.concat(t, ","))
LUAEOF

# -- luar -b (LuaJIT CLI compatibility) -----------------------------------
cat > "$TMP/cross.lua" <<'LUAEOF'
local t = {}
for i = 1, 5 do t[i] = i * i end
print(table.concat(t, ","))
LUAEOF
"$LUAR" -b "$TMP/cross.lua" "$TMP/cross-b.luac" || fail "luar -b"
out=$("$LUAR" "$TMP/cross-b.luac")
[ "$out" = "1,4,9,16,25" ] || fail "luar -b output: $out"
"$LUAR" -b -s "$TMP/cross.lua" "$TMP/cross-bs.luac" || fail "luar -b -s"
out=$("$LUAR" "$TMP/cross-bs.luac")
[ "$out" = "1,4,9,16,25" ] || fail "luar -b -s output: $out"
ok "luar -b"

# -- cross-interpreter bytecode compatibility ------------------------------
"$LUARC" -o "$TMP/cross-luar.luac" "$TMP/cross.lua"
if command -v lua >/dev/null 2>&1; then
  out=$(lua "$TMP/cross-luar.luac")
  [ "$out" = "1,4,9,16,25" ] || fail "luarc output on stock lua: $out"
fi
if command -v luac >/dev/null 2>&1; then
  luac -o "$TMP/cross-stock.luac" "$TMP/cross.lua"
  out=$("$LUAR" "$TMP/cross-stock.luac")
  [ "$out" = "1,4,9,16,25" ] || fail "stock luac output on luar: $out"
fi
ok "bytecode is interchangeable with Lua 5.5.1"

# -- LUAR_INIT precedence --------------------------------------------------
out=$(LUAR_INIT='print("luar-init")' LUA_INIT='print("lua-init")' "$LUAR" -e '')
[ "$out" = "luar-init" ] || fail "LUAR_INIT not honoured: $out"
out=$(LUA_INIT='print("lua-init")' "$LUAR" -e '')
[ "$out" = "lua-init" ] || fail "LUA_INIT fallback broken: $out"
ok "LUAR_INIT"

# -- embedding through the public headers ---------------------------------
# libffi is dlopen()ed by the FFI module, so a static link needs no -lffi.
cc -O2 -I "$ROOT/src" -o "$TMP/embed" "$ROOT/tests/embed/embed.c" \
   "$ROOT/src/libluar.a" -lm -ldl ||
  fail "could not build the embed test"
out=$("$TMP/embed") || fail "embed test failed"
[ "$out" = "embed ok" ] || fail "embed output: $out"
ok "C embedding"

# -- C module ABI (needs pkg-config lua5.5) --------------------------------
if pkg-config --exists lua5.5 2>/dev/null; then
  # shellcheck disable=SC2046  # pkg-config output is a flag list
  cc -O2 -fPIC -shared $(pkg-config --cflags lua5.5) \
     -o "$TMP/mhello.so" "$ROOT/tests/cmod/mhello.c" \
     $(pkg-config --libs lua5.5) 2>"$TMP/mhello.err" || {
       cat "$TMP/mhello.err" >&2; fail "could not build the ABI test module"; }
  out=$(LUA_CPATH="$TMP/?.so" "$LUAR" -e 'local m=require"mhello"; print(m.hello())')
  case "$out" in
    "hello from a stock lua5.5 module	505") ;;
    *) fail "stock Lua 5.5 C module did not load/run: $out" ;;
  esac
  ok "Lua 5.5 C module ABI"
else
  echo "cli: skip: pkg-config lua5.5 not available (C module ABI)"
fi

echo "cli: all checks passed"
