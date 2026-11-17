# Testing LuaR

LuaR treats the official Lua implementation as the specification, so most of
the test strategy is about proving that LuaR has not drifted from it.

## Quick start

```sh
make test              # smoke + CLI/ABI + semantics + compat + differential
make test-semantics    # tests/semantics/*.lua
make test-compat       # tests/compat/*.lua
make test-diff         # same scripts under stock lua 5.5 and luar
make test-ffi          # tests/ffi/*.lua (requires WITH_FFI=1)
tools/upstream-check.sh
tests/upstream/run.sh  # official Lua test suite (downloads once)
make bench             # performance harness, no pass/fail gates
```

## Suites

| Path | Purpose | Must pass under stock Lua? |
| --- | --- | --- |
| `tests/smoke.lua` | identity, extension surface, 5.5 basics | no (uses `luar`/`jit`/`bit`) |
| `tests/semantics/` | language and stdlib conformance | **yes** |
| `tests/diff/` | byte-for-byte output comparison vs stock `lua` | yes by construction |
| `tests/compat/` | LuaJIT-compatible extension API | no |
| `tests/ffi/` | FFI parser, cdata, calls | no |
| `tests/cli.sh` | binaries, bytecode, ABI, embedding | no |
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
belong here as well as in `tests/semantics/`.

The semantics files are also expected to pass under stock Lua. CI runs them
that way to catch tests that accidentally encode LuaR behaviour.

## ABI tests

`tests/cli.sh` compiles `tests/cmod/mhello.c` with the system `lua5.5`
headers and loads it into `luar`, proving the module ABI. It also compiles
`tests/embed/embed.c` against `src/libluar.a` and `luar.h`, proving the
embedding API and that `luaL_openlibs` brings up the extensions.

When the shared library lands (milestone 0.3), the same module must load
against `libluar.so` through `luar.pc`.

## Official suite

`tests/upstream/run.sh` downloads the Lua authors' test suite for the exact
base version into an ignored scratch directory, runs it with `luar`, and
writes `tests/tmp/upstream-report.txt`. It is intentionally not vendored:
the suite changes with Lua releases and is not part of LuaR.

## FFI tests

The FFI suite is built and run only with `WITH_FFI=1`. It must be
deterministic and depend only on libc. Prefer computing sizes/offsets from a
tiny C helper built at test time over hard-coding them, so the suite is
portable across x86-64 and ARM64. Run with sanitizers when touching the cdata
runtime:

```sh
make clean
make WITH_FFI=1 CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
ASAN_OPTIONS=detect_leaks=0 ./src/luar tests/run.lua ffi
```

(Leak checking is disabled because the Lua state owns allocations that are
released at `lua_close`, which is fine; crashes and OOB accesses are not.)

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
