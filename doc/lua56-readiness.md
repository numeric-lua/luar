# Lua 5.6 readiness

LuaR targets **one Lua line at a time**.  Today that line is 5.5.1; when PUC
publishes 5.6, adopting it is a deliberate, tested migration rather than a
compatibility mode.  This page is the checklist for that migration.

## Assumptions

* LuaR has no 5.1–5.4 compatibility layer and will not gain one for 5.6:
  `global` stays reserved, deprecated math functions stay absent, and no
  `LUA_COMPAT_*` switches are reintroduced.
* The upstream C API/ABI, bytecode format and standard library are the
  semantic authority.  LuaR extensions must keep working on top.
* The 5.5-only test suite (`tests/semantics`, `tests/compat/strict55.lua`)
  is updated, not bypassed, if upstream changes language rules.
* The FFI is a LuaR extension; its C ABI assumptions (libffi, pointer
  widths, `TValue` layout) must be revalidated against the new core.

## Adoption sequence

1. **Snapshot.**  Fetch and verify the official tarball:

   ```sh
   tools/upstream-sync.sh 5.6.0            # dry run: fetch, verify, diff
   tools/upstream-sync.sh 5.6.0 --apply    # install vendor/lua-5.6.0/
   ```

2. **Merge the core.**  Apply the vendor diff to `src/` (three-way), keeping
   LuaR edits tagged `/* LUAR */`.  Do not edit `vendor/`.

3. **Baseline.**  Review every changed tracked file and refresh:

   ```sh
   tools/upstream-check.sh --update
   tools/upstream-check.sh
   ```

4. **Build matrix.**

   ```sh
   make clean && make && make test
   make WITH_FFI=1 test
   make JIT=0 test
   make shared test-shared
   make test-asan
   make test-fuzz
   tests/upstream/run.sh --jobs 4
   ```

5. **JIT review.**  The J2 whitelist is deliberately explicit
   (`src/luar_jit.c` opcode switches and `src/luar_emit_x64.dasc`).  For each
   new/removed/changed opcode:
   * compile it only if its semantics on the 16-byte `TValue` slots are
     exactly the interpreter's;
   * otherwise leave it out and let the loop fall back — never guess.
   Re-run `tests/jit` and the official suite with `jit.on()`; check
   `tools/upstream-baseline.txt` for `lobject.h`/`lvm.c` changes.

6. **FFI review.**  Rebuild `tests/ffi/helper.c` and run the FFI suites,
   including the real `libluatorch.so` consumer.  Verify `ffi.sizeof`,
   `offsetof`, enum/bitfield decisions, varargs and callbacks against the
   new core; update `doc/ffi.md` deviations.

7. **5.5-only rules.**  Search for removed/renamed 5.5 constructs and update
   `tests/compat/strict55.lua` and `doc/lua55-only.md`.  A 5.6 program must
   not silently depend on 5.5 behaviours we chose to drop.

8. **Performance.**  Re-run `make bench-compare` and refresh the J2 numbers
   in `doc/performance.md`; the stock-VM rows should still track upstream
   within measurement noise.

9. **Docs and release.**  Update `README.md`, `CHANGELOG.md`,
   `doc/roadmap.md`, `doc/upstream.md` and `src/luar_version.h`; bump
   `LUAR_VERSION`/soname only for ABI-breaking changes.

## Known 5.6-sensitive areas

| Area | Why it matters | Owner |
| --- | --- | --- |
| `lobject.h` `TValue`/`Proto` layout | J2 hard-codes 16-byte slots and tag offset 8; it already refuses other layouts | JIT |
| opcode table and `lvm.c` dispatch | J2 recorder/JIT decode opcodes by number/format | JIT |
| integer/float configuration | J2 requires 64-bit integers and double floats | JIT/build |
| libffi and C ABI | variadic/callback promotions depend on the host ABI, not Lua | FFI |
| removed 5.5 features | strict 5.5 tests must move with upstream, not be deleted silently | tests |

Until 5.6 exists, `tools/upstream-check.sh` keeps the 5.5.1 baseline green
and the official 5.5 suite is the release gate.
