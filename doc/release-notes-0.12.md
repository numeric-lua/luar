# Luar 0.12 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
brings hash insertion into J4 table-write traces.

## Highlights

- **Hash insertion/rehash TSET**: J4 can now insert integer keys into the
  hash part of a table, including rehash growth, using a Lua-state-aware
  helper.  The loop ABI now passes `lua_State *` alongside `base`.
- **Metatable safety**: absent keys with `__newindex` still use interpreter
  fallback; existing cells remain raw writes.
- **Benchmark**: `j4/hash_insert` fills a fresh table through the traced path.
- `tests/jit/66_hash_insert.lua` verifies 1000 inserted keys and values.
- J4 suite now has 65 files.

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

GitHub Actions also runs the macOS shared/install/module job.

## Known limitations

- Native exit-stub spilling remains C-level snapshot/restore.
- FFI-in-trace specialization is not implemented.
- Hash deletion/boundary rehash edge cases rely on Lua’s own table routines.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
