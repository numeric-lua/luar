#!/bin/sh
# tests/upstream/run.sh -- run the official Lua test suite under Luar.
#
# The suite is downloaded from https://www.lua.org/tests/ into the ignored
# tests/tmp/ directory, unpacked there, and run with ./src/luar.  It is never
# committed.
#
# Each test file is run in its own private copy of the suite (so --jobs N is
# safe), followed by the suite's own all.lua harness in "basic" mode
# (_U=true, the mode documented by lua.org for portable systems).  Results
# are written to tests/tmp/upstream-report.txt.
#
# Usage: tests/upstream/run.sh [--jobs N] [--offline]

set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/../.." && pwd)
LUAR=${LUAR:-$ROOT/src/luar}
SCRATCH=$ROOT/tests/tmp/upstream
WORK=$SCRATCH/work
REPORT=$ROOT/tests/tmp/upstream-report.txt
JOBS=
OFFLINE=0

usage() {
  cat <<'EOF'
usage: tests/upstream/run.sh [--jobs N] [--offline]

  --jobs N    run N test files in parallel (default: min(CPUs, 8))
  --offline   do not download; require the cached suite tarball
  -h, --help  show this help

The official suite and all logs live under tests/tmp/ (git-ignored).  The
summary is written to tests/tmp/upstream-report.txt.
EOF
}

die() {
  echo "upstream-tests: $*" >&2
  exit 2
}

while [ $# -gt 0 ]; do
  case $1 in
    --jobs)
      [ $# -ge 2 ] || die "--jobs needs a number"
      JOBS=$2
      shift
      ;;
    --jobs=*)
      JOBS=${1#*=}
      ;;
    --offline|--no-network) OFFLINE=1 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown argument: $1 (try --help)" ;;
  esac
  shift
done

if [ -n "$JOBS" ]; then
  case $JOBS in
    *[!0-9]*|'') die "--jobs must be a positive integer" ;;
  esac
  [ "$JOBS" -ge 1 ] || die "--jobs must be a positive integer"
else
  JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
  case $JOBS in
    *[!0-9]*|'') JOBS=1 ;;
  esac
  [ "$JOBS" -ge 1 ] || JOBS=1
  [ "$JOBS" -le 8 ] || JOBS=8
fi

[ -d "$ROOT/src" ] || die "cannot find src/ under $ROOT"

# Keep the interpreter in step with src/; make is a fast no-op when it is
# already current.  A LUAR override is respected and not rebuilt.
if [ "$LUAR" = "$ROOT/src/luar" ]; then
  echo "upstream-tests: ensuring src/luar is built"
  ( cd "$ROOT" && make )
fi
[ -x "$LUAR" ] || die "no runnable interpreter at $LUAR"

# The suite version comes from the Lua base version recorded by Luar.
VERSION=$(sed -n \
  's/^#define[[:space:]]*LUAR_LUA_VERSION[[:space:]]*"Lua \([0-9][0-9.]*\)".*/\1/p' \
  "$ROOT/src/luar_version.h" | head -n 1)
if [ -z "$VERSION" ]; then
  VERSION=$("$LUAR" -v 2>&1 | sed -n 's/.*Lua \([0-9][0-9.]*\).*/\1/p' | head -n 1)
fi
[ -n "$VERSION" ] || die "cannot determine the Lua version"

SUITE_NAME=lua-$VERSION-tests
SUITE_URL=https://www.lua.org/tests/$SUITE_NAME.tar.gz
TARBALL=$SCRATCH/$SUITE_NAME.tar.gz
SUITE=$SCRATCH/$SUITE_NAME

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

mkdir -p "$SCRATCH"

TARBALL_OK=0
if [ -f "$TARBALL" ] && tar -tzf "$TARBALL" >/dev/null 2>&1; then
  TARBALL_OK=1
fi

if [ "$TARBALL_OK" -eq 0 ]; then
  if [ "$OFFLINE" -eq 1 ]; then
    die "offline mode: no cached suite tarball at $TARBALL"
  fi
  echo "upstream-tests: fetching $SUITE_URL"
  PART=$TARBALL.part
  rm -f "$PART"
  download "$SUITE_URL" "$PART"
  tar -tzf "$PART" >/dev/null 2>&1 || {
    rm -f "$PART"
    die "downloaded test suite is not a valid tar.gz archive"
  }
  mv "$PART" "$TARBALL"
fi

SHA=$(hash_file "$TARBALL")
echo "upstream-tests: suite $SUITE_NAME"
echo "upstream-tests: sha256 $SHA"

if [ ! -f "$SUITE/all.lua" ]; then
  rm -rf "$SUITE"
  tar -xzf "$TARBALL" -C "$SCRATCH" || die "could not unpack $TARBALL"
fi
[ -f "$SUITE/all.lua" ] || die "suite did not unpack to $SUITE"

# Luar is strict Lua 5.5: 'global' is always a reserved word and there is no
# LUA_COMPAT_GLOBAL.  The upstream basic suite still takes its compatibility
# branch when T is nil, so skip that one assertion in the working copy.  This
# is the same configuration upstream's own ltests build uses
# (ltests/ltests.h undefines LUA_COMPAT_GLOBAL).
if grep -q 'when not in "test mode", "global" isn' "$SUITE/goto.lua"; then
  sed -i 's/if not T then  -- when not in "test mode", "global" isn.t reserved/if false then  -- Luar strict 5.5: global is always reserved/' \
    "$SUITE/goto.lua"
fi

rm -rf "$WORK"
mkdir -p "$WORK/bin" "$WORK/logs" "$WORK/status" "$WORK/runs"

# Some tests assume a program literally named "lua" on PATH.
rm -f "$WORK/bin/lua"
ln -s "$LUAR" "$WORK/bin/lua"
PATH=$WORK/bin:$PATH
export PATH

# Reproduce the globals all.lua sets up for _U (portable/basic) mode.  The
# test files are normally loaded by all.lua; running one directly needs the
# same knobs and the Message() helper.
PRELUDE=$WORK/prelude.lua
cat > "$PRELUDE" <<'EOF'
_soft = true
_port = true
_nomsg = true
ARG = arg
T = nil
debug = nil
function Message (_m) end
EOF

TEST_LIST=$WORK/tests.list
: > "$TEST_LIST"
for f in "$SUITE"/*.lua; do
  [ -f "$f" ] || continue
  name=${f##*/}
  case $name in
    all.lua|tracegc.lua|heavy.lua) continue ;;
  esac
  printf '%s\n' "$name" >> "$TEST_LIST"
done

TEST_COUNT=$(wc -l < "$TEST_LIST" | tr -d ' ')
[ "$TEST_COUNT" -gt 0 ] || die "no test files found in $SUITE"

TIMEOUT=300
if ! command -v timeout >/dev/null 2>&1; then
  TIMEOUT=
fi

RUNROOT=$WORK/runs
LOGDIR=$WORK/logs
STATUSDIR=$WORK/status

# Run one test file in its own suite copy.  The exit status is recorded in a
# file, so a crashed worker still produces a visible result.
run_one() {
  name=$1
  base=${name%.lua}
  rundir=$RUNROOT/$base
  log=$LOGDIR/$base.log
  status=$STATUSDIR/$base.status

  if mkdir -p "$rundir" && cp -R "$SUITE"/. "$rundir"/; then
    if (
      cd "$rundir" || exit 125
      if [ -n "$TIMEOUT" ]; then
        timeout "$TIMEOUT" "$LUAR" -e "dofile('$PRELUDE')" "$name" > "$log" 2>&1
      else
        "$LUAR" -e "dofile('$PRELUDE')" "$name" > "$log" 2>&1
      fi
    ); then
      rc=0
    else
      rc=$?
    fi
  else
    rc=125
    : > "$log"
  fi
  printf '%s\n' "$rc" > "$status"
}

echo "upstream-tests: running $TEST_COUNT files with --jobs $JOBS"
running=0
while IFS= read -r name; do
  run_one "$name" &
  running=$((running + 1))
  if [ "$running" -ge "$JOBS" ]; then
    wait || true
    running=0
  fi
done < "$TEST_LIST"
wait || true

# The suite's own driver is the authoritative end-to-end run.  Feed stdin
# from a pipe so the non-portable seek test in files.lua behaves the same way
# on a terminal-less CI machine.
CANON_LOG=$WORK/all.log
if (
  cd "$SUITE" || exit 125
  if [ -n "$TIMEOUT" ]; then
    : | timeout "$TIMEOUT" "$LUAR" -e"_U=true" all.lua > "$CANON_LOG" 2>&1
  else
    : | "$LUAR" -e"_U=true" all.lua > "$CANON_LOG" 2>&1
  fi
); then
  CANON_RC=0
else
  CANON_RC=$?
fi

if [ "$CANON_RC" -eq 0 ] && grep -q 'final OK !!!' "$CANON_LOG"; then
  CANON_STATUS=PASS
else
  CANON_STATUS=FAIL
fi

LUAR_ID=$("$LUAR" -v 2>&1 | sed -n '1p')
case $LUAR_ID in
  *"Lua $VERSION"*) ;;
  *) echo "upstream-tests: warning: interpreter says '$LUAR_ID', expected Lua $VERSION" >&2 ;;
esac

: > "$REPORT"

emit() {
  printf '%s\n' "$*" >> "$REPORT"
  printf '%s\n' "$*"
}

emit "# Luar official upstream test-suite report"
emit "# suite:        $SUITE_NAME"
emit "# url:          $SUITE_URL"
emit "# tarball:      tests/tmp/upstream/$SUITE_NAME.tar.gz"
emit "# sha256:       $SHA"
emit "# interpreter:  $LUAR_ID"
emit "# mode:         official basic tests (_U=true; non-portable tests skipped)"
emit "# adaptation:   strict 5.5 -- goto.lua's LUA_COMPAT_GLOBAL branch disabled"
emit "# note:         PASS means the file harness completed; in _U mode some"
emit "#               non-portable checks (notably main.lua) return early by design"
emit "# jobs:         $JOBS"
emit "# canonical:    $CANON_STATUS (all.lua: final OK marker)"
emit ""

PASS_N=0
FAIL_N=0
TIMEOUT_N=0
MISSING_N=0
DETAILS=$WORK/details.txt
: > "$DETAILS"

while IFS= read -r name; do
  base=${name%.lua}
  status_file=$STATUSDIR/$base.status
  log_file=$LOGDIR/$base.log
  if [ ! -f "$status_file" ]; then
    rc=missing
    label='FAIL (no status: worker crashed)'
    MISSING_N=$((MISSING_N + 1))
    FAIL_N=$((FAIL_N + 1))
  else
    rc=$(cat "$status_file")
    case $rc in
      0) label=PASS; PASS_N=$((PASS_N + 1)) ;;
      124) label='TIMEOUT'; TIMEOUT_N=$((TIMEOUT_N + 1)); FAIL_N=$((FAIL_N + 1)) ;;
      *) label="FAIL (exit $rc)"; FAIL_N=$((FAIL_N + 1)) ;;
    esac
  fi
  emit "$label  $name"
  if [ "$rc" != 0 ] && [ -f "$log_file" ]; then
    emit "  log: tests/tmp/upstream/work/logs/$base.log"
    {
      echo "### $name"
      tail -n 5 "$log_file"
      echo
    } >> "$DETAILS"
  fi
done < "$TEST_LIST"

emit ""
emit "# per-file: $TEST_COUNT run, $PASS_N passed, $FAIL_N failed" \
     "($TIMEOUT_N timed out, $MISSING_N missing status)"
emit "# canonical: $CANON_STATUS (exit $CANON_RC)"
if [ "$CANON_STATUS" = FAIL ]; then
  emit ""
  emit "# canonical all.lua log tail:"
  tail -n 20 "$CANON_LOG" | sed 's/^/#   /' >> "$REPORT"
  tail -n 20 "$CANON_LOG" | sed 's/^/#   /'
fi

if [ -s "$DETAILS" ]; then
  emit ""
  emit "# failure details (last 5 lines per file):"
  sed 's/^/# /' "$DETAILS" >> "$REPORT"
fi

echo
echo "upstream-tests: per-file $PASS_N/$TEST_COUNT passed, $FAIL_N failed; canonical $CANON_STATUS"
echo "upstream-tests: report written to tests/tmp/upstream-report.txt"

if [ "$FAIL_N" -gt 0 ] || [ "$CANON_STATUS" != PASS ]; then
  exit 1
fi
exit 0
