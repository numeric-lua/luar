# Luar 0.13 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
completes hash insertion coverage for both roots and chained sides.

## Highlights

- **Side-segment hash insertion**: a side entered at a closure-identity guard
  can insert new integer keys into an initially empty hash table, including
  rehash growth, using the Lua-state-aware TSET helper.
- `tests/jit/67_side_hash_insert.lua` runs the root against one table and the
  side against a second empty table, then verifies all 1000 keys.
- `j4/hash_insert` benchmark fills a fresh table through the traced path.
- J4 suite now has 66 files.

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
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
