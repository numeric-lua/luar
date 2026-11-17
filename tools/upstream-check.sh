#!/bin/sh
# tools/upstream-check.sh -- compare src/ with the vendored Lua tree.
#
# LuaR keeps the official Lua sources in vendor/lua-<version>/ untouched and
# carries its own changes in src/.  This script distinguishes:
#
#   * files LuaR added (luar_*.c/h, luar.h, luar_version.h);
#   * files LuaR modified, which must be recorded in
#     tools/upstream-baseline.txt together with both hashes;
#   * files that are missing from src/ or unexpectedly present.
#
# Build products in src/ are ignored.  The check is offline.
#
# Exit status: 0 = no unexpected drift, 1 = drift, 2 = usage/configuration.
#
# See doc/upstream.md for the policy.

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
BASELINE="$ROOT/tools/upstream-baseline.txt"
SRC="$ROOT/src"
VENDOR_ROOT="$ROOT/vendor"

UPDATE=0
VENDOR_ARG=

usage() {
  cat <<'EOF'
usage: tools/upstream-check.sh [--update] [--vendor DIR]

  --update       refresh tools/upstream-baseline.txt from the current tree
  --vendor DIR   compare against DIR instead of the newest vendor/lua-*
  -h, --help     show this help

The check never writes to src/ or vendor/.  --update only rewrites the
baseline, and refuses if upstream files are missing or unexpected extra
files are present.
EOF
}

die() {
  echo "upstream-check: $*" >&2
  exit 2
}

while [ $# -gt 0 ]; do
  case $1 in
    --update) UPDATE=1 ;;
    --vendor)
      [ $# -ge 2 ] || die "--vendor needs a directory"
      VENDOR_ARG=$2
      shift
      ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1 (try --help)" ;;
  esac
  shift
done

[ -d "$SRC" ] || die "no src/ directory at $SRC"

if [ -n "$VENDOR_ARG" ]; then
  case $VENDOR_ARG in
    /*) VENDOR=$VENDOR_ARG ;;
    *) VENDOR=$ROOT/$VENDOR_ARG ;;
  esac
else
  # Pick the newest vendored upstream snapshot by numeric version, not by
  # lexical order (5.10 must sort after 5.9).
  VENDOR_VER=$(for d in "$VENDOR_ROOT"/lua-*; do
    [ -d "$d" ] || continue
    printf '%s\n' "${d##*/lua-}"
  done | sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1)
  [ -n "$VENDOR_VER" ] || die "no vendor/lua-* directory found"
  VENDOR=$VENDOR_ROOT/lua-$VENDOR_VER
fi

VENDOR=${VENDOR%/}
[ -d "$VENDOR/src" ] || die "no upstream source at $VENDOR/src"
VENDOR_NAME=${VENDOR##*/}
[ -n "$VENDOR_NAME" ] || die "cannot determine the upstream directory name"

# sha256 differs between GNU coreutils, BSD/macOS userland and OpenSSL.
if command -v sha256sum >/dev/null 2>&1; then
  SHA_TOOL=sha256sum
elif command -v shasum >/dev/null 2>&1; then
  SHA_TOOL=shasum
elif command -v openssl >/dev/null 2>&1; then
  SHA_TOOL=openssl
else
  die "no sha256 tool found (need sha256sum, shasum or openssl)"
fi

hash_file() {
  case $SHA_TOOL in
    sha256sum) sha256sum "$1" | cut -d' ' -f1 ;;
    shasum) shasum -a 256 "$1" | cut -d' ' -f1 ;;
    openssl) openssl dgst -sha256 "$1" | awk '{print $NF}' ;;
  esac
}

is_luar_added() {
  case $1 in
    luar_*.c|luar_*.h|luar_*.dasc|luar.h) return 0 ;;
  esac
  return 1
}

is_ignored_artifact() {
  case $1 in
    *.o|*.a|*.so|*.so.*|*.dylib|*.dll|*.obj|*.lib|*.exp|*.pdb|*.dSYM) return 0 ;;
    *.gcda|*.gcno|*.profraw|*.out|luar|luarc|.prefix|.ffi|.jit|*.d|*.pic.d|.DS_Store) return 0 ;;
  esac
  return 1
}

TMP=$(mktemp -d "${TMPDIR:-/tmp}/luar-upstream-check.XXXXXX") || die "mktemp failed"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

VENDOR_LIST=$TMP/vendor.list
SRC_LIST=$TMP/src.list
MODIFIED=$TMP/modified.list
MISSING=$TMP/missing.list
EXTRA=$TMP/extra.list
ADDED=$TMP/added.list
IGNORED=$TMP/ignored.list
PROBLEMS=$TMP/problems.list
CURRENT=$TMP/current-modified.list
UPSTREAM_MOVED=0

: > "$MODIFIED"
: > "$MISSING"
: > "$EXTRA"
: > "$ADDED"
: > "$IGNORED"
: > "$PROBLEMS"
: > "$CURRENT"

# List regular files in a flat directory (the Lua source tree is flat).
list_files() {
  dir=$1
  out=$2
  : > "$out"
  for p in "$dir"/* "$dir"/.[!.]* "$dir"/..?*; do
    [ -f "$p" ] || continue
    printf '%s\n' "${p##*/}" >> "$out"
  done
  sort -o "$out" "$out"
}

add_problem() {
  printf '%s\n' "$1" >> "$PROBLEMS"
}

list_files "$VENDOR/src" "$VENDOR_LIST"
list_files "$SRC" "$SRC_LIST"

VENDOR_COUNT=$(wc -l < "$VENDOR_LIST" | tr -d ' ')
SRC_COUNT=$(wc -l < "$SRC_LIST" | tr -d ' ')

# --- upstream files: unchanged, modified, or missing -----------------------
while IFS= read -r name; do
  if [ ! -f "$SRC/$name" ]; then
    printf '%s\n' "$name" >> "$MISSING"
  elif ! cmp -s "$VENDOR/src/$name" "$SRC/$name"; then
    printf '%s\n' "$name" >> "$MODIFIED"
  fi
done < "$VENDOR_LIST"

# --- files only in src/: LuaR additions, build products, or unexpected -----
while IFS= read -r name; do
  [ -f "$VENDOR/src/$name" ] && continue
  if is_luar_added "$name"; then
    printf '%s\n' "$name" >> "$ADDED"
  elif is_ignored_artifact "$name"; then
    printf '%s\n' "$name" >> "$IGNORED"
  else
    printf '%s\n' "$name" >> "$EXTRA"
  fi
done < "$SRC_LIST"

# --- read the baseline ------------------------------------------------------
BASELINE_UPSTREAM=
BASELINE_ROWS=$TMP/baseline.data
: > "$BASELINE_ROWS"
if [ -f "$BASELINE" ]; then
  BASELINE_UPSTREAM=$(awk '$1 == "upstream" && NF == 2 { print $2; exit }' "$BASELINE")
  awk 'NF == 3 && $1 !~ /^#/ { print $1, $2, $3 }' "$BASELINE" > "$BASELINE_ROWS"
elif [ "$UPDATE" -eq 0 ]; then
  add_problem "baseline is missing: $BASELINE (run --update after reviewing modified files)"
fi

if [ -n "$BASELINE_UPSTREAM" ] && [ "$BASELINE_UPSTREAM" != "$VENDOR_NAME" ]; then
  add_problem "baseline records '$BASELINE_UPSTREAM' but comparing '$VENDOR_NAME' (review the upstream change, then run --update)"
fi

# --- classify each modified file against the baseline ----------------------
while IFS= read -r name; do
  shash=$(hash_file "$SRC/$name")
  vhash=$(hash_file "$VENDOR/src/$name")
  line=$(awk -v p="$name" '$3 == p { print; exit }' "$BASELINE_ROWS")
  if [ -z "$line" ]; then
    state=unrecorded
    add_problem "modified file not in baseline: src/$name (run --update after review)"
  else
    bhash=$(printf '%s\n' "$line" | awk '{print $1}')
    buphash=$(printf '%s\n' "$line" | awk '{print $2}')
    if [ "$buphash" != "$vhash" ]; then
      state=upstream-mismatch
      UPSTREAM_MOVED=$((UPSTREAM_MOVED + 1))
      add_problem "baseline upstream hash is stale for src/$name: vendored file changed since the baseline was made (review upstream, then run --update)"
    elif [ "$bhash" != "$shash" ]; then
      state=source-changed
      add_problem "src/$name changed since the baseline was recorded (run --update after reviewing the edit)"
    else
      state=tracked
    fi
  fi
  printf '%s %s %s %s\n' "$shash" "$vhash" "$name" "$state" >> "$CURRENT"
done < "$MODIFIED"

# --- baseline rows that no longer describe a modified file -----------------
if [ -s "$BASELINE_ROWS" ]; then
  while IFS=' ' read -r bhash buphash bpath; do
    [ -n "$bpath" ] || continue
    if ! awk -v p="$bpath" '$3 == p { found = 1 } END { exit !found }' "$CURRENT"; then
      add_problem "baseline entry '$bpath' is stale: the file is no longer modified (or no longer exists); run --update"
    fi
  done < "$BASELINE_ROWS"
fi

# --- missing / extra are always problems -----------------------------------
while IFS= read -r name; do
  add_problem "upstream file missing from src/: src/$name"
done < "$MISSING"
while IFS= read -r name; do
  add_problem "unexpected file in src/: src/$name (LuaR additions must be luar_*, luar.h or luar_version.h)"
done < "$EXTRA"

# --- report -----------------------------------------------------------------
printf '== LuaR upstream check ==\n'
printf 'upstream : %s/src (%s files)\n' "$VENDOR" "$VENDOR_COUNT"
printf 'source   : %s (%s files)\n' "$SRC" "$SRC_COUNT"
printf 'baseline : %s\n' "$BASELINE"
printf '\n'

ADDED_COUNT=$(wc -l < "$ADDED" | tr -d ' ')
MODIFIED_COUNT=$(wc -l < "$MODIFIED" | tr -d ' ')
IGNORED_COUNT=$(wc -l < "$IGNORED" | tr -d ' ')

printf 'LuaR-added files (%s):\n' "$ADDED_COUNT"
if [ "$ADDED_COUNT" -eq 0 ]; then
  printf '  (none)\n'
else
  sed 's/^/  /' "$ADDED"
fi
printf '\n'

printf 'Modified from upstream (%s):\n' "$MODIFIED_COUNT"
if [ "$MODIFIED_COUNT" -eq 0 ]; then
  printf '  (none)\n'
else
  while IFS=' ' read -r shash vhash name state; do
    case $state in
      tracked) label=tracked ;;
      unrecorded) label='UNRECORDED (review, then --update)' ;;
      source-changed) label='CHANGED since baseline (review, then --update)' ;;
      upstream-mismatch) label='UPSTREAM CHANGED (review, then --update)' ;;
      *) label=$state ;;
    esac
    printf '  %-24s %s\n' "$name" "$label"
  done < "$CURRENT"
fi
printf '\n'

if [ -s "$MISSING" ] || [ -s "$EXTRA" ]; then
  printf 'Missing / extra files:\n'
  sed 's/^/  missing from src: /' "$MISSING"
  sed 's/^/  extra in src:     /' "$EXTRA"
  printf '\n'
fi

if [ "$IGNORED_COUNT" -gt 0 ]; then
  printf 'Build products ignored: %s\n\n' "$IGNORED_COUNT"
fi

PROBLEM_COUNT=$(wc -l < "$PROBLEMS" | tr -d ' ')

if [ "$UPDATE" -eq 1 ]; then
  if [ -s "$MISSING" ] || [ -s "$EXTRA" ]; then
    printf 'Problems:\n'
    sed 's/^/  - /' "$PROBLEMS"
    printf '\n'
    echo "upstream-check: refusing --update: fix missing/extra files first" >&2
    exit 1
  fi

  BASELINE_NEW=$TMP/baseline.new
  {
    echo "# LuaR upstream baseline -- generated by tools/upstream-check.sh --update."
    echo "# Do not edit by hand.  Each row is:"
    echo "#   <sha256 of src/>  <sha256 of vendored upstream>  <path>"
    echo "# See doc/upstream.md for the patch/baseline policy."
    echo "upstream $VENDOR_NAME"
    while IFS=' ' read -r shash vhash name _state; do
      printf '%s  %s  %s\n' "$shash" "$vhash" "$name"
    done < "$CURRENT"
  } > "$BASELINE_NEW"
  mv "$BASELINE_NEW" "$BASELINE"

  if [ -n "$BASELINE_UPSTREAM" ] && [ "$BASELINE_UPSTREAM" != "$VENDOR_NAME" ]; then
    echo "upstream-check: warning: baseline was for $BASELINE_UPSTREAM, now recording $VENDOR_NAME" >&2
    echo "upstream-check: warning: review the upstream diff before committing the baseline" >&2
  elif [ "$UPSTREAM_MOVED" -gt 0 ]; then
    echo "upstream-check: warning: $UPSTREAM_MOVED tracked file(s) had changed upstream;" >&2
    echo "upstream-check: warning: review the upstream diff before committing the baseline" >&2
  fi
  printf 'Updated %s (%s tracked modification(s)).\n' "$BASELINE" "$MODIFIED_COUNT"
  echo "upstream-check: OK (baseline refreshed)"
  exit 0
fi

if [ "$PROBLEM_COUNT" -gt 0 ]; then
  printf 'Problems (%s):\n' "$PROBLEM_COUNT"
  sed 's/^/  - /' "$PROBLEMS"
  printf '\n'
  echo "upstream-check: FAILED: unexpected drift from $VENDOR_NAME" >&2
  exit 1
fi

printf 'Summary: %s modified (all tracked), %s added, 0 missing, 0 extra; %s build product(s) ignored.\n' \
  "$MODIFIED_COUNT" "$ADDED_COUNT" "$IGNORED_COUNT"
echo "upstream-check: OK"
exit 0
