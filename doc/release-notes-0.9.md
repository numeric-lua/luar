# Luar 0.9 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
brings table writes into chained J4 side traces.

## Highlights

- **TSET-safe side chains**: side and child segments may now contain integer
  table writes.  A side failure no longer restores and replays a prefix that
  already mutated the table; it resumes the interpreter at the exact failing
  PC, so each write is applied exactly once.
- **TSET chain test**: `tests/jit/63_side_tset.lua` changes an inlined
  closure identity so the child side performs `t[i] = i`, then verifies every
  element and the accumulated result.
- J4 suite now has 62 files.

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

GitHub Actions also runs the macOS shared/install job.

## Known limitations

- Hash/`__newindex`/growing table writes still fall back; side TSET is the
  integer array-cell fast path.
- Blocked-PC tracking is bitmap-based and conservative.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
