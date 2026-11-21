# Testing Luar

Luar treats the official Lua implementation as the specification, so most of
the test strategy is about proving that Luar has not drifted from it.

## Quick start

```sh
make test              # smoke + CLI/ABI + semantics + compat + differential
make test-shared       # shared-library embedding, C module ABI, luar.pc
make test-semantics    # tests/semantics/*.lua
make test-compat       # tests/compat/*.lua
make test-diff         # same scripts under stock lua 5.5 and luar
make test-ffi          # tests/ffi/*.lua (requires WITH_FFI=1)
make test-asan         # clean ASan+UBSan build, suites and cdef fuzz
make test-fuzz         # deterministic ffi.cdef fuzzer (builds WITH_FFI=1)
tools/upstream-check.sh
tests/upstream/run.sh  # official Lua test suite (downloads once)
make bench             # performance harness, no pass/fail gates
make bench-jit         # JIT-on vs JIT-off report for the j4 group
```

## Suites

| Path | Purpose | Must pass under stock Lua? |
| --- | --- | --- |
| `tests/smoke.lua` | identity, extension surface, 5.5 basics | no (uses `luar`/`jit`) |
| `tests/semantics/` | language and stdlib conformance | **yes** |
| `tests/diff/` | byte-for-byte output comparison vs stock `lua` | yes by construction |
| `tests/compat/` | LuaJIT-style extensions and strict-55 rules | no |
| `tests/ffi/` | FFI parser, cdata, calls | no |
| `tests/consumer/` | real `libluatorch.so` over FFI (skips if absent) | n/a |
| `tests/cli.sh` | binaries, bytecode, ABI, embedding | no |
| `tests/shared.sh` | `libluar.so` soname, exports, embedding, C module ABI | no |
| `tests/upstream/` | official Lua authors' suite | it is the reference |

`tests/run.lua` runs every `.lua` file in the requested suite directories and
fails on any raised error. Test files are plain chunks; use
`tests/helpers.lua` for assertions:

```lua
local H = dofile("tests/helpers.lua")
H.eq(got, want, "message")
H.near(got, want, 1e-9)
H.raises(function () ... end, "pattern")
```

## The differential guard

`tests/diff/run.sh` runs each script under both `lua` (stock 5.5.1) and
`src/luar`, then compares stdout and exit status. The scripts must be
deterministic: no addresses, no `pairs` order, no wall-clock output. This is
the cheapest and strongest anti-fork test; new language-visible changes
belong here as well as in `tests/semantics/`. If no stock `lua` is on
`PATH`, the runner prints a skip and exits successfully.

The semantics files are also expected to pass under stock Lua. CI runs them
that way to catch tests that accidentally encode Luar behaviour.

## ABI tests

`tests/cli.sh` compiles `tests/cmod/mhello.c` with the system `lua5.5`
headers and loads it into `luar`, proving the module ABI. It also compiles
`tests/embed/embed.c` against `src/libluar.a` and `luar.h`, proving the
embedding API and that `luaL_openlibs` brings up the extensions.

`tests/shared.sh` (run by `make test-shared`) repeats the embedding test
against `src/libluar.so` with `-L src -lluar` and `LD_LIBRARY_PATH=src`, and
links a copy of the CLI itself against the shared library. It checks the
soname chain (`libluar.so` → `libluar.so.0` → `libluar.so.0.98.0`), verifies
with `nm -D` that only `lua_*`/`luaL_*`/`luaopen_*`/`luar_*` are exported,
loads the stock `mhello.c` module into the shared-linked CLI, and exercises
both `pkg-config --libs luar` and `pkg-config --static --libs luar` against
the checked-in `etc/luar.pc` (with `libdir`/`includedir` pointed at the
source tree and a temporary static-only directory).

The stock module is linked with `-llua` and therefore carries
`DT_NEEDED liblua.so.5.5`. This does not cause interposition: `libluar.so`
is a direct dependency of the host and therefore precedes the module's own
dependency in the global lookup scope, so the module's `lua_*` references
bind to Luar. On glibc the test confirms this with `LD_DEBUG=bindings`;
elsewhere the working module is the evidence. The same reasoning explains
why a *static* host must be linked with `-Wl,-E` to load C modules at all:
it has no other way to publish `lua_*` to `dlopen`.

## Official suite

`tests/upstream/run.sh` downloads the Lua authors' test suite for the exact
base version into an ignored scratch directory, runs it with `luar`, and
writes `tests/tmp/upstream-report.txt`. It is intentionally not vendored:
the suite changes with Lua releases and is not part of Luar.

## FFI tests

The FFI suite is built and run only with `WITH_FFI=1`. It must be
deterministic and depend only on libc. Prefer computing sizes/offsets from a
tiny C helper built at test time over hard-coding them, so the suite is
portable across x86-64 and ARM64. Run `tests/asan.sh` (or `make test-asan`)
when touching the cdata runtime; it builds with ASan+UBSan and includes the
FFI suite. See [Sanitizers](#sanitizers-asan--ubsan).

## Sanitizers (ASan + UBSan)

`tests/asan.sh` is the supported sanitizer entry point:

```sh
make test-asan                 # clean ASan+UBSan run, FFI build (default)
make WITH_FFI=0 test-asan      # stub build: semantics/compat/differential only
tests/asan.sh                  # same as the first form
WITH_FFI=0 tests/asan.sh       # same as the second form
CC=clang tests/asan.sh         # optional compiler override
```

The script runs `make clean`, then rebuilds with:

```sh
CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra"
LDFLAGS="-fsanitize=address,undefined"
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

and runs the semantic and compat suites, the FFI suite when `WITH_FFI=1`,
the differential guard, and (for FFI builds) the cdef fuzzer. Any sanitizer
diagnostic aborts the run.

Generated code is entered through raw machine-code pointers, which carry
none of the type metadata that Clang's `-fsanitize=function` check (part of
`-fsanitize=undefined`) reads just before an indirect call target.  The
functions in `src/luar_jit.c` that enter generated code therefore carry
`no_sanitize("function")`; every other sanitizer still instruments them, and
the emitted code itself is not instrumented.

The script restores nothing: the tree is left with an ASan+UBSan build. Run
`make clean` before the next normal build; this also clears the
sanitizer flags, which make cannot detect by itself.

Leak detection is disabled because a Lua state owns its allocations until
`lua_close()`; treating unreachable-at-shutdown blocks as leaks adds noise
without covering the runtime. Address errors, use-after-free, out-of-bounds
accesses and undefined behaviour are still fatal.

`tests/asan.sh` intentionally does not run `tests/cli.sh`: the embedding
check compiles `tests/embed/embed.c` against `src/libluar.a` without the
sanitizer runtime, so that link would not exercise the instrumented build.
Run the CLI/ABI checks with a normal build.

## cdef fuzzing

`tools/fuzz-cdef.lua` is a deterministic grammar- and mutation-based fuzzer
for the `ffi.cdef` declaration parser:

```sh
make test-fuzz                  # builds WITH_FFI=1, runs 5000 cases
./src/luar tools/fuzz-cdef.lua  # same, against an existing FFI build
./src/luar tools/fuzz-cdef.lua --cases 20000 --seed 0x1234 --max-seconds 10
```

A fixed seed feeds a splitmix64 stream, so a run with the same arguments is
byte-for-byte identical. Cases come from a corpus of known-tricky
declarations (nested arrays, anonymous structs/unions, unterminated comments
and strings, huge integer constants, long pointer chains), random
truncations and byte/token mutations of that corpus, random token soup, and
bounded structured aggregate declarations. Each case is passed to
`ffi.cdef` under `pcall`: a Lua error is the expected result for invalid
input; a crash, abort, hang, or non-string error fails the run.

`make test-fuzz` forces an FFI build (the FFI is opt-in) and wraps the
fuzzer in `timeout(1)` when available. The script also checks a CPU-time
budget between cases; the external wrapper is what catches a hang inside the
C parser. `tests/asan.sh` runs the same fuzzer under ASan+UBSan.

The fuzzer has no dependency beyond the FFI build and takes about a quarter
of a second for the default 5000 cases in a normal build (about a second
under ASan); it is not coverage-guided and does not exercise cdata
allocation or calls.

## Continuous integration

`.github/workflows/ci.yml` gates the Linux matrix: gcc with `WITH_FFI=0`
and `WITH_FFI=1`, clang with `WITH_FFI=1`, a `JIT=0` build, a staged
`make install` plus `pkg-config` and uninstall checks, a shared-library/FFI
job, a clean
ASan/UBSan job (`tests/asan.sh`), a deterministic fuzz job
(`make test-fuzz`), and the offline `tools/upstream-check.sh` drift check.
shellcheck runs when available. The macOS job is optional (`continue-on-error`) and builds the `.dylib`
shared library, then builds and runs the full `WITH_FFI=1` suite; the latter
downloads its inputs from lua.org. Run `tests/upstream/run.sh --offline` locally for the vendored/cached
path.

## JIT testing (from milestone 0.4)

Every trace must be validated differentially before it runs: record on the
interpreter, re-execute with the same inputs, compare results and observable
effects (metamethod calls, errors). Guard exits and side traces get the same
treatment. `jit.flush()`/`jit.off()` are part of the test matrix, not an
afterthought.

## Benchmarks

`bench/` is not a gate. Results are recorded in `doc/performance.md` with
machine, compiler and date. Report losses explicitly; a benchmark that only
shows wins is marketing, not engineering.

## Adding a test

1. Pick the suite: semantics (upstream behaviour), compat (LuaJIT surface),
   ffi, or diff (cross-interpreter equality).
2. Keep it deterministic and independent of the network and wall clock.
3. Fail with a useful message; do not print and continue.
4. Run it under both interpreters when it belongs to semantics/diff.
