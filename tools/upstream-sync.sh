#!/bin/sh
# tools/upstream-sync.sh -- fetch a new official Lua release and compare it
# with the current vendor snapshot and LuaR's src/ tree.
#
# Usage:
#   tools/upstream-sync.sh 5.6.0            # fetch, unpack, report, no writes
#   tools/upstream-sync.sh 5.6.0 --offline  # use a cached tarball only
#   tools/upstream-sync.sh 5.6.0 --apply    # install vendor/lua-5.6.0
#   tools/upstream-sync.sh 5.6.0 --check-url
#
# The default is a dry run.  --apply only installs the pristine vendor tree;
# merging the upstream changes into src/ is deliberately left to a human
# (see doc/upstream.md).  An existing vendor tree is never overwritten
# silently: --apply refuses unless the tree is already identical, and
# --force first moves the old tree into an ignored backup directory.
#
# Cached downloads and extracted snapshots live under tests/tmp/upstream-sync/
# (ignored by git).  --offline never touches the network.

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
CACHE=${LUAR_UPSTREAM_CACHE:-$ROOT/tests/tmp/upstream-sync}
VENDOR_ROOT=$ROOT/vendor
SRC=$ROOT/src

APPLY=0
OFFLINE=0
CHECK_URL=0
FORCE=0
EXPECT_SHA=
FROM=
VERSION=

usage() {
  cat <<'EOF'
usage: tools/upstream-sync.sh <version> [options]

  --apply         install the released sources as vendor/lua-<version>/
  --offline       never use the network; require a cached tarball
  --check-url     only verify that the download URL exists
  --sha256 HASH   require the downloaded tarball to have this sha256
  --from DIR      compare against DIR instead of the current vendor tree
  --force         with --apply, back up and replace an existing vendor tree
  -h, --help      show this help

The version must be a full Lua release such as 5.5.1 or 5.6.0.  Tarballs are
downloaded from https://www.lua.org/ftp/lua-<version>.tar.gz.
EOF
}

die() {
  echo "upstream-sync: $*" >&2
  exit 2
}

warn() {
  echo "upstream-sync: warning: $*" >&2
}

while [ $# -gt 0 ]; do
  case $1 in
    --apply) APPLY=1 ;;
    --offline|--no-network) OFFLINE=1 ;;
    --check-url) CHECK_URL=1 ;;
    --force) FORCE=1 ;;
    --sha256)
      [ $# -ge 2 ] || die "--sha256 needs a value"
      EXPECT_SHA=$2
      shift
      ;;
    --from)
      [ $# -ge 2 ] || die "--from needs a directory"
      FROM=$2
      shift
      ;;
    -h|--help) usage; exit 0 ;;
    -*) die "unknown option: $1 (try --help)" ;;
    *)
      [ -z "$VERSION" ] || die "more than one version given"
      VERSION=$1
      ;;
  esac
  shift
done

[ -n "$VERSION" ] || { usage >&2; exit 2; }
case $VERSION in
  *[!0-9.]*|.*|*..*|*.) die "invalid version '$VERSION' (expected X.Y.Z)" ;;
esac
printf '%s\n' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' ||
  die "invalid version '$VERSION' (expected X.Y.Z)"

if [ -n "$EXPECT_SHA" ]; then
  case $EXPECT_SHA in
    *[!0-9A-Fa-f]*) die "--sha256 is not a hex digest" ;;
  esac
  EXPECT_SHA=$(printf '%s\n' "$EXPECT_SHA" | tr 'A-F' 'a-f')
fi

URL="https://www.lua.org/ftp/lua-$VERSION.tar.gz"
TARBALL=$CACHE/lua-$VERSION.tar.gz
UNPACK_ROOT=$CACHE/unpacked
NEW=$UNPACK_ROOT/lua-$VERSION
TARGET=$VENDOR_ROOT/lua-$VERSION

if [ "$CHECK_URL" -eq 1 ] && [ "$OFFLINE" -eq 1 ]; then
  die "--check-url needs the network; do not combine it with --offline"
fi

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

check_url() {
  if command -v curl >/dev/null 2>&1; then
    code=$(curl -fsSIL -o /dev/null -w '%{http_code}' --max-time 30 "$URL") ||
      die "could not reach $URL"
    echo "url:    $URL"
    echo "status: HTTP $code"
  elif command -v wget >/dev/null 2>&1; then
    CHECK_TMP=$(mktemp "${TMPDIR:-/tmp}/luar-check-url.XXXXXX") || die "mktemp failed"
    if wget --spider -S "$URL" > "$CHECK_TMP" 2>&1; then
      sed -n '1,12p' "$CHECK_TMP"
      rm -f "$CHECK_TMP"
      echo "url:    $URL"
    else
      cat "$CHECK_TMP"
      rm -f "$CHECK_TMP"
      die "could not reach $URL"
    fi
  else
    die "need curl or wget for --check-url"
  fi
}

download() {
  url=$1
  dest=$2
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --retry 3 --connect-timeout 20 -o "$dest" "$url" ||
      die "download failed: $url"
  elif command -v wget >/dev/null 2>&1; then
    wget -O "$dest" "$url" ||
      die "download failed: $url"
  else
    die "need curl or wget to download $url"
  fi
}

is_ignored_artifact() {
  case $1 in
    *.o|*.a|*.so|*.so.*|*.dylib|*.dll|*.obj|*.lib|*.exp|*.pdb|*.dSYM) return 0 ;;
    *.gcda|*.gcno|*.profraw|*.out|luar|luarc|.prefix|.ffi|.DS_Store) return 0 ;;
  esac
  return 1
}

is_luar_added() {
  case $1 in
    luar_*.c|luar_*.h|luar.h) return 0 ;;
  esac
  return 1
}

list_sources() {
  dir=$1
  out=$2
  : > "$out"
  for p in "$dir"/* "$dir"/.[!.]* "$dir"/..?*; do
    [ -f "$p" ] || continue
    name=${p##*/}
    if [ "$dir" = "$SRC" ]; then
      is_ignored_artifact "$name" && continue
    fi
    printf '%s\n' "$name" >> "$out"
  done
  sort -o "$out" "$out"
}

tree_equal() {
  diff -r "$1" "$2" >/dev/null 2>&1
}

if [ "$CHECK_URL" -eq 1 ]; then
  check_url
  exit 0
fi

mkdir -p "$CACHE"

if [ "$OFFLINE" -eq 1 ]; then
  [ -f "$TARBALL" ] ||
    die "offline mode: no cached tarball at $TARBALL (run once online first)"
  echo "offline: using cached $TARBALL"
else
  PART=$TARBALL.part
  rm -f "$PART"
  echo "fetching: $URL"
  download "$URL" "$PART" || { rm -f "$PART"; exit 2; }
  if ! tar -tzf "$PART" >/dev/null 2>&1; then
    rm -f "$PART"
    die "downloaded file is not a valid gzip tar archive"
  fi
  GOT_SHA=$(hash_file "$PART")
  if [ -n "$EXPECT_SHA" ] && [ "$GOT_SHA" != "$EXPECT_SHA" ]; then
    rm -f "$PART"
    die "sha256 mismatch: expected $EXPECT_SHA, got $GOT_SHA"
  fi
  mv "$PART" "$TARBALL"
fi

[ -f "$TARBALL" ] || die "missing tarball $TARBALL"
tar -tzf "$TARBALL" >/dev/null 2>&1 || die "cached tarball is corrupt: $TARBALL"
SHA=$(hash_file "$TARBALL")
if [ -n "$EXPECT_SHA" ] && [ "$SHA" != "$EXPECT_SHA" ]; then
  die "sha256 mismatch for $TARBALL: expected $EXPECT_SHA, got $SHA"
fi

echo "version: $VERSION"
echo "url:     $URL"
echo "sha256:  $SHA"
echo "tarball: $TARBALL"

# Unpack into the ignored cache.  Validate the expected top-level directory
# before touching any vendor path.
rm -rf "$NEW"
mkdir -p "$UNPACK_ROOT"
tar -xzf "$TARBALL" -C "$UNPACK_ROOT" || die "could not unpack $TARBALL"
[ -d "$NEW/src" ] || die "unexpected archive layout: $NEW/src not found"
[ -f "$NEW/src/lua.h" ] || die "unexpected archive layout: $NEW/src/lua.h not found"
echo "unpacked: $NEW"

# Pick the old vendor tree.  Refreshing the same version compares the target
# itself with the new download; a new version compares against the newest
# existing snapshot (use --from to override).
OLD=
if [ -n "$FROM" ]; then
  case $FROM in
    /*) OLD=$FROM ;;
    *) OLD=$ROOT/$FROM ;;
  esac
  [ -d "$OLD/src" ] || die "--from is not a Lua source tree: $OLD"
elif [ -d "$TARGET" ]; then
  OLD=$TARGET
else
  OLD_VER=$(for d in "$VENDOR_ROOT"/lua-*; do
    [ -d "$d" ] || continue
    printf '%s\n' "${d##*/lua-}"
  done | sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1)
  if [ -n "$OLD_VER" ]; then
    OLD=$VENDOR_ROOT/lua-$OLD_VER
  fi
fi

if [ -n "$OLD" ]; then
  echo "old:     $OLD"
else
  echo "old:     (none; this will be the first vendored snapshot)"
fi
echo

TMP=$(mktemp -d "${TMPDIR:-/tmp}/luar-upstream-sync.XXXXXX") || die "mktemp failed"
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

OLD_LIST=$TMP/old.list
NEW_LIST=$TMP/new.list
SRC_LIST=$TMP/src.list
: > "$OLD_LIST"
: > "$NEW_LIST"
: > "$SRC_LIST"
if [ -n "$OLD" ]; then
  list_sources "$OLD/src" "$OLD_LIST"
fi
list_sources "$NEW/src" "$NEW_LIST"
list_sources "$SRC" "$SRC_LIST"

UP_MOD=$TMP/up.modified
UP_ADD=$TMP/up.added
UP_DEL=$TMP/up.removed
LR_MOD=$TMP/lr.modified
LR_ADD=$TMP/lr.added
CONFLICT=$TMP/conflicts
MISSING=$TMP/missing
STALE=$TMP/stale
: > "$UP_MOD"
: > "$UP_ADD"
: > "$UP_DEL"
: > "$LR_MOD"
: > "$LR_ADD"
: > "$CONFLICT"
: > "$MISSING"
: > "$STALE"

UP_MOD_N=0
UP_ADD_N=0
UP_DEL_N=0
LR_MOD_N=0
LR_ADD_N=0
CONFLICT_N=0
MISSING_N=0
STALE_N=0

if [ -z "$OLD" ]; then
  echo "Three-way summary: no old vendor tree to compare against."
  echo "  new vendor: $NEW"
  echo "  source:     $SRC"
  echo "  (the first snapshot cannot classify upstream vs LuaR changes;"
  echo "   use --from DIR to compare against an unpacked older release)"
  echo
else
  ALL=$TMP/all.list
  cat "$OLD_LIST" "$NEW_LIST" "$SRC_LIST" | sort -u > "$ALL"

  while IFS= read -r name; do
    in_old=0
    in_new=0
    in_src=0
    [ -f "$OLD/src/$name" ] && in_old=1
    [ -f "$NEW/src/$name" ] && in_new=1
    [ -f "$SRC/$name" ] && in_src=1

    up=0
    if [ "$in_old" -eq 1 ] && [ "$in_new" -eq 1 ]; then
      cmp -s "$OLD/src/$name" "$NEW/src/$name" || up=1
    elif [ "$in_old" -eq 0 ] && [ "$in_new" -eq 1 ]; then
      up=1
      printf '%s\n' "$name" >> "$UP_ADD"
    elif [ "$in_old" -eq 1 ] && [ "$in_new" -eq 0 ]; then
      up=1
      printf '%s\n' "$name" >> "$UP_DEL"
    fi
    if [ "$up" -eq 1 ] && [ "$in_old" -eq 1 ] && [ "$in_new" -eq 1 ]; then
      printf '%s\n' "$name" >> "$UP_MOD"
    fi

    lr=0
    if [ "$in_old" -eq 1 ] && [ "$in_src" -eq 1 ]; then
      cmp -s "$OLD/src/$name" "$SRC/$name" || lr=1
      [ "$lr" -eq 1 ] && printf '%s\n' "$name" >> "$LR_MOD"
    elif [ "$in_old" -eq 1 ] && [ "$in_src" -eq 0 ]; then
      lr=1
      printf '%s\n' "$name" >> "$LR_MOD"
    elif [ "$in_old" -eq 0 ] && [ "$in_src" -eq 1 ]; then
      lr=1
      printf '%s\n' "$name" >> "$LR_ADD"
    fi

    if [ "$up" -eq 1 ] && [ "$lr" -eq 1 ]; then
      printf '%s\n' "$name" >> "$CONFLICT"
    fi

    # New upstream files absent from src/ must be added during the rebase.
    if [ "$in_old" -eq 0 ] && [ "$in_new" -eq 1 ] && [ "$in_src" -eq 0 ]; then
      printf '%s\n' "$name" >> "$MISSING"
    fi
    # Files removed upstream but still present in src/ must be dealt with.
    if [ "$in_old" -eq 1 ] && [ "$in_new" -eq 0 ] && [ "$in_src" -eq 1 ]; then
      printf '%s\n' "$name" >> "$STALE"
    fi
  done < "$ALL"

  count() {
    wc -l < "$1" | tr -d ' '
  }

  UP_MOD_N=$(count "$UP_MOD")
  UP_ADD_N=$(count "$UP_ADD")
  UP_DEL_N=$(count "$UP_DEL")
  LR_MOD_N=$(count "$LR_MOD")
  LR_ADD_N=$(count "$LR_ADD")
  CONFLICT_N=$(count "$CONFLICT")
  MISSING_N=$(count "$MISSING")
  STALE_N=$(count "$STALE")

  echo "Three-way summary (old vendor -> new vendor, compared with src/):"
  echo "  old vendor: $OLD"
  echo "  new vendor: $NEW"
  echo "  source:     $SRC"
  echo
  echo "  upstream changes: $((UP_MOD_N + UP_ADD_N + UP_DEL_N))" \
       "(modified $UP_MOD_N, added $UP_ADD_N, removed $UP_DEL_N)"
  echo "  LuaR changes vs old vendor: $((LR_MOD_N + LR_ADD_N))" \
       "(modified/deleted $LR_MOD_N, added $LR_ADD_N)"
  echo "  conflicts requiring a manual rebase: $CONFLICT_N"

  print_list() {
    label=$1
    file=$2
    n=$3
    [ "$n" -gt 0 ] || return 0
    echo "    $label:"
    sed 's/^/      /' "$file"
  }

  print_list "modified upstream" "$UP_MOD" "$UP_MOD_N"
  print_list "added upstream" "$UP_ADD" "$UP_ADD_N"
  print_list "removed upstream" "$UP_DEL" "$UP_DEL_N"
  print_list "changed by LuaR" "$LR_MOD" "$LR_MOD_N"
  print_list "LuaR-added files" "$LR_ADD" "$LR_ADD_N"
  print_list "conflicts" "$CONFLICT" "$CONFLICT_N"
  print_list "new upstream files missing from src" "$MISSING" "$MISSING_N"
  print_list "removed upstream but still in src" "$STALE" "$STALE_N"
  echo
fi

if [ "$APPLY" -eq 0 ]; then
  echo "Dry run: no files changed.  The extracted snapshot is under tests/tmp/."
  echo "Re-run with --apply to install vendor/lua-$VERSION/, then rebase src/ by hand."
  exit 0
fi

if [ "$CONFLICT_N" -gt 0 ] || [ "$MISSING_N" -gt 0 ] || [ "$STALE_N" -gt 0 ]; then
  warn "$CONFLICT_N conflict(s), $MISSING_N new file(s) and $STALE_N stale file(s) need a manual rebase after the vendor copy"
fi

if [ -d "$TARGET" ]; then
  if tree_equal "$TARGET" "$NEW"; then
    echo "vendor/lua-$VERSION is already identical to the downloaded release; nothing to do."
    exit 0
  fi
  if [ "$FORCE" -ne 1 ]; then
    die "refusing to overwrite existing $TARGET: it differs from the downloaded release.
Move it aside yourself, or use --force to back it up under tests/tmp/ first."
  fi
  BACKUP=$CACHE/backup/lua-$VERSION.$(date +%Y%m%d%H%M%S)
  mkdir -p "$CACHE/backup"
  mv "$TARGET" "$BACKUP"
  echo "backed up: $BACKUP"
fi

cp -R "$NEW" "$TARGET"
tree_equal "$NEW" "$TARGET" || die "copy verification failed for $TARGET"
echo "installed: $TARGET"
echo "src/ was not modified.  Rebase it against vendor/lua-$VERSION/src/, then run"
echo "tools/upstream-check.sh --update and the test suites (see doc/upstream.md)."
