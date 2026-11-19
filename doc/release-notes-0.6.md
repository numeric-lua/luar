# LuaR 0.6 release notes

LuaR is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
generalizes chained J4 side traces to the configured depth cap.

## Highlights

- **Depth-4 side chains**: a root side can chain through up to three child
  segments (depth 2, 3 and 4), each entered at the exact failure PC of its
  parent and reusing the parent’s interpreter-consistent state.
- **No-progress blocking**: a child that would start at or before its parent
  start is blocked for the epoch; repeated failures cannot spin a chain.
- **Depth tests**: `tests/jit/60_side_chain_depth.lua` exercises a
  four-closure chain and asserts depth 4 is reached.
- Existing exact root/side exit, mixed int/float, float-table and CALL-exit
  coverage remains green.

## Verification

```sh
make test
make WITH_FFI=1 test
make JIT=0 test
make test-asan
make test-fuzz
make shared test-shared
tests/upstream/run.sh
make dist && make test-dist
```

GitHub Actions covers GCC/Clang, FFI on/off, JIT=0, ASan/UBSan,
deterministic fuzz, staged install/pkg-config, shared-library and the
official Lua 5.5.1 suite.

## Known limitations

- Chained side segments remain limited to the four-node cap.
- `TSET` and unsupported float arithmetic are excluded from side chains and
  fall back.
- Blocked PCs are conservative (hash-bit collisions only lose optimization).
- Native exit-stub spilling and full macOS shared ABI tests remain.
- LuaR supports only Lua 5.5; no 5.1–5.4 compatibility mode.
