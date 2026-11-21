#!/bin/sh
# tests/shared.sh -- shared-library build, ABI and distribution checks.
#
# Run by 'make test-shared' (which builds static + shared first).  The
# embedding host and the CLI are linked with -L src -lluar and run with
# LD_LIBRARY_PATH=src, the same lookup an installed host gets from /usr/lib.
#
# This is Linux/ELF-oriented; the parts that need readelf/nm are skipped on
# platforms without them.
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
SRC="$ROOT/src"
TMP="$ROOT/tests/tmp/shared"
CC=${CC:-cc}

mkdir -p "$TMP"
fail() { echo "shared: FAIL: $*" >&2; exit 1; }
ok() { echo "shared: ok: $*"; }
skip() { echo "shared: skip: $*"; }

run_shared() {
  LD_LIBRARY_PATH="$SRC${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$@"
}

# -- artifact names and soname ---------------------------------------------
[ -f "$SRC/libluar.so" ] || fail "src/libluar.so missing (run 'make shared')"
[ -L "$SRC/libluar.so" ] || fail "src/libluar.so is not a symlink"
link=$(readlink "$SRC/libluar.so")
[ "$link" = "libluar.so.0" ] ||
  fail "libluar.so points to '$link', expected libluar.so.0"
full=$(readlink "$SRC/$link")
[ -n "$full" ] || fail "$link is not a symlink"
[ -f "$SRC/$full" ] || fail "$link is dangling (no $full)"
case "$full" in
  libluar.so.0.*) ;;
  *) fail "unexpected real library name: $full" ;;
esac
ok "symlinks: libluar.so -> $link -> $full"

if command -v readelf >/dev/null 2>&1; then
  soname=$(readelf -d "$SRC/$full" |
    sed -n 's/.*(SONAME).*\[\(.*\)\]/\1/p')
  [ "$soname" = "libluar.so.0" ] || fail "SONAME is '$soname', expected libluar.so.0"
  if readelf -d "$SRC/$full" | grep -q 'NEEDED.*liblua'; then
    fail "libluar.so depends on a system liblua"
  fi
  if readelf -d "$SRC/$full" | grep -q 'NEEDED.*libffi'; then
    fail "libluar.so links libffi (the FFI uses dlopen and must not add -lffi)"
  fi
  ok "SONAME $soname; NEEDED has no liblua/libffi"
else
  skip "readelf not available (SONAME/NEEDED)"
fi

# -- exported symbol set ----------------------------------------------------
if nm -D --defined-only "$SRC/$full" >/dev/null 2>&1; then
  SYMS="$TMP/dynsyms.txt"
  nm -D --defined-only "$SRC/$full" | awk '{print $NF}' | sort -u > "$SYMS"
  bad=$(grep -vE '^(lua_|luaL_|luaopen_)' "$SYMS" |         grep -vxE 'luar_(version|lua_version|os|arch|has_ffi|has_jit|openlibs)' || true)
  [ -z "$bad" ] || fail "non-API symbols exported: $bad"
  for sym in luaL_newstate lua_newstate luaL_openselectedlibs luaopen_base \
             luaopen_table luaopen_luar luaopen_jit luaopen_ffi \
             luar_version luar_lua_version luar_openlibs luar_has_ffi; do
    grep -qx "$sym" "$SYMS" || fail "missing exported symbol: $sym"
  done
  if [ -f "$SRC/libluar.a" ]; then
    STATIC_SYMS="$TMP/staticsyms.txt"
    { nm -g --defined-only "$SRC/libluar.a" |
        awk '$2 ~ /^[TDBR]$/ {print $3}' |
        grep -E '^(lua_|luaL_|luaopen_)'
      nm -g --defined-only "$SRC/libluar.a" |
        awk '$2 ~ /^[TDBR]$/ {print $3}' |
        grep -xE 'luar_(version|lua_version|os|arch|has_ffi|has_jit|openlibs)'
    } | sort -u > "$STATIC_SYMS"
    if ! diff -u "$STATIC_SYMS" "$SYMS" > "$TMP/export-diff.txt"; then
      cat "$TMP/export-diff.txt" >&2
      fail "dynamic exports do not match the static Luar API"
    fi
    ok "dynamic exports match the static API set exactly"
  fi
  ok "$(wc -l < "$SYMS" | tr -d ' ') exported symbols, all lua_*/luaL_*/luaopen_*/luar_*"
else
  skip "nm -D not available (export check)"
fi

# -- embedding host linked against the shared library ----------------------
# shellcheck disable=SC2086  # intentionally unquoted compiler invocation
$CC -O2 -Wall -Wextra -I "$SRC" -o "$TMP/embed-shared" \
  "$ROOT/tests/embed/embed.c" -L "$SRC" -lluar -lm ||
  fail "could not link tests/embed/embed.c against libluar.so"
out=$(run_shared "$TMP/embed-shared") || fail "shared embed host failed"
[ "$out" = "embed ok" ] || fail "shared embed output: $out"
ok "C host links -Lsrc -lluar, calls luar_version()/luaL_openlibs/Lua code"

if command -v ldd >/dev/null 2>&1; then
  line=$(LD_LIBRARY_PATH="$SRC" ldd "$TMP/embed-shared" |
    grep 'libluar\.so\.0' || true)
  case "$line" in
    *"$SRC/"*) ;;
    *) fail "embed host does not resolve the in-tree libluar.so.0: ${line:-<missing>}" ;;
  esac
  ok "ldd: $(printf '%s' "$line" | sed 's/^[[:space:]]*//')"
fi

# -- the luar CLI itself linked against the shared library -----------------
$CC -O2 -Wall -Wextra -I "$SRC" -o "$TMP/luar-shared" \
  "$SRC/lua.o" -L "$SRC" -lluar -lm ||
  fail "could not link src/lua.o against libluar.so"
out=$(run_shared "$TMP/luar-shared" -v)
case "$out" in
  "Luar 0.73.0 (Lua 5.5.1)"*) ;;
  *) fail "shared-linked luar -v output: $out" ;;
esac
out=$(run_shared "$TMP/luar-shared" -e \
  'print(luar.version .. "|" .. luar.lua_version .. "|" .. (6*7))')
[ "$out" = "0.73.0|Lua 5.5.1|42" ] || fail "shared-linked luar output: $out"
ok "luar CLI runs against libluar.so and reports the Luar identity"

# -- stock Lua 5.5 C module loaded through the shared library --------------
# Built against the system lua5.5 when available, exactly like tests/cli.sh.
if pkg-config --exists lua5.5 2>/dev/null; then
  # shellcheck disable=SC2046  # pkg-config output is a flag list
  $CC -O2 -fPIC -shared $(pkg-config --cflags lua5.5) \
    -o "$TMP/mhello.so" "$ROOT/tests/cmod/mhello.c" \
    $(pkg-config --libs lua5.5) || fail "could not build the stock C module"
  MOD_DESC="stock lua5.5 headers + liblua.so"
else
  $CC -O2 -fPIC -shared -I "$SRC" \
    -o "$TMP/mhello.so" "$ROOT/tests/cmod/mhello.c" ||
    fail "could not build the C module from Luar headers"
  MOD_DESC="Luar headers (pkg-config lua5.5 not available)"
fi
out=$(run_shared env LUA_CPATH="$TMP/?.so" "$TMP/luar-shared" -e \
  'local m = require"mhello"; print(m.hello())') ||
  fail "stock C module did not load into the shared-linked luar"
case "$out" in
  "hello from a stock lua5.5 module"*"505") ;;
  *) fail "C module output: $out" ;;
esac
ok "C module ABI through libluar.so ($MOD_DESC)"

# The stock module carries DT_NEEDED liblua.so.5.5, but the executable's
# libluar.so comes first in the global lookup scope, so lua_* bind to Luar.
# Confirm that on glibc where LD_DEBUG is available; otherwise the working
# module plus the DT_NEEDED check above is the observable evidence.
if command -v ldd >/dev/null 2>&1 &&
   ldd --version 2>&1 | grep -qiE 'glibc|GNU libc'; then
  LD_DEBUG=bindings LD_LIBRARY_PATH="$SRC" LUA_CPATH="$TMP/?.so" \
    "$TMP/luar-shared" -e 'require"mhello"' 2>"$TMP/bindings.log" >/dev/null ||
    fail "LD_DEBUG run of the C module failed"
  if grep -q 'mhello\.so.*liblua\.so' "$TMP/bindings.log"; then
    fail "mhello.so bound lua_* symbols to the system liblua.so"
  fi
  if grep -q 'mhello\.so.*libluar\.so' "$TMP/bindings.log"; then
    ok "mhello.so binds its lua_* symbols to libluar.so (no interposition)"
  else
    skip "could not confirm the mhello -> libluar binding via LD_DEBUG"
  fi
fi

# -- pkg-config: shared and static link recipes ----------------------------
if command -v pkg-config >/dev/null 2>&1; then
  pc_version=$(PKG_CONFIG_PATH="$ROOT/etc" pkg-config \
    --define-variable=includedir="$SRC" --modversion luar)
  hdr_version=$(sed -n 's/^#define LUAR_VERSION[[:space:]]*"\(.*\)"/\1/p' "$SRC/luar_version.h")
  [ "$pc_version" = "$hdr_version" ] ||
    fail "pkg-config version ($pc_version) != luar_version.h ($hdr_version)"
  pc_cflags=$(PKG_CONFIG_PATH="$ROOT/etc" pkg-config \
    --define-variable=includedir="$SRC" --cflags luar)
  pc_libs=$(PKG_CONFIG_PATH="$ROOT/etc" pkg-config \
    --define-variable=libdir="$SRC" --libs luar)
  case "$pc_libs" in
    *-lluar*) ;;
    *) fail "pkg-config --libs luar does not pass -lluar: $pc_libs" ;;
  esac
  # shellcheck disable=SC2086  # pkg-config output is a flag list
  $CC -O2 -Wall -Wextra $pc_cflags -o "$TMP/embed-pc" \
    "$ROOT/tests/embed/embed.c" $pc_libs ||
    fail "linking through 'pkg-config --cflags/--libs luar' failed"
  out=$(run_shared "$TMP/embed-pc") || fail "pkg-config-linked host failed"
  [ "$out" = "embed ok" ] || fail "pkg-config-linked host output: $out"
  ok "pkg-config --cflags/--libs luar links the shared library"

  # Force the static archive through a libdir that holds only libluar.a, so
  # --static is actually exercised rather than silently picking libluar.so.
  mkdir -p "$TMP/static-lib"
  cp "$SRC/libluar.a" "$TMP/static-lib/libluar.a"
  static_libs=$(PKG_CONFIG_PATH="$ROOT/etc" pkg-config \
    --define-variable=libdir="$TMP/static-lib" --define-variable=includedir="$SRC" \
    --static --libs luar)
  case "$static_libs" in
    *-lluar*) ;;
    *) fail "pkg-config --static --libs luar does not pass -lluar: $static_libs" ;;
  esac
  if [ "$(uname -s)" != "Darwin" ]; then
    case "$static_libs" in
      *-ldl*) ;;
      *) fail "pkg-config --static --libs luar lacks Libs.private (-ldl): $static_libs" ;;
    esac
  fi
  # shellcheck disable=SC2086  # pkg-config output is a flag list
  $CC -O2 -Wall -Wextra $pc_cflags -o "$TMP/embed-pc-static" \
    "$ROOT/tests/embed/embed.c" $static_libs ||
    fail "static link through 'pkg-config --static --libs luar' failed"
  out=$("$TMP/embed-pc-static") || fail "pkg-config static host failed"
  [ "$out" = "embed ok" ] || fail "pkg-config static host output: $out"
  ok "pkg-config --static --libs luar links libluar.a"
else
  skip "pkg-config not available (luar.pc checks)"
fi

echo "shared: all checks passed"
