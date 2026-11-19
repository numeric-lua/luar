# Luar 0.10 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
extends side-trace table writes from array cells to existing hash cells.

## Highlights

- **Hash-cell TSET**: side segments can write integer values to an existing
  integer key in a table’s hash part.  Missing keys, metatables and rehashes
  still fall back to the interpreter.
- **Exact side fallback**: hash writes are applied once; no replay of a
  prefix that already mutated the table.
- `tests/jit/64_side_hash_tset.lua` verifies writes and reads for keys
  1001–2000 after the root falls back to a side.
- J4 suite now has 63 files.

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

- Hash TSET requires an existing cell; insertion/rehash uses interpreter
  fallback.
- Metatable-bearing tables intentionally fall back.
- Native exit-stub spilling remains C-level snapshot/restore.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
