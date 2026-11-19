# Luar 0.11 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
finishes the current table-write side-trace work and validates C modules on
macOS.

## Highlights

- **Metatable-aware side TSET**: an existing array or hash cell is written
  raw even when the table has a metatable, matching Lua’s fast path.  An empty
  array cell with `__newindex` still falls back to the interpreter.
- **macOS C module**: CI compiles a loadable module against
  `libluar.dylib`, loads it through `require`, and checks its return value.
- `tests/jit/65_side_meta_tset.lua` covers array and hash writes under a
  counting `__newindex`; the metamethod must not run for existing cells.
- J4 suite now has 64 files.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

GitHub Actions also runs macOS shared-dylib, staged-install/pkg-config and
C-module load checks.

## Known limitations

- Hash insertion/rehash still uses interpreter fallback.
- Native exit-stub spilling remains C-level snapshot/restore.
- FFI-in-trace specialization is not implemented.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
