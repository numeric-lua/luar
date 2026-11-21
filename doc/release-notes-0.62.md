# Luar 0.62 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
starts trace-side allocation with a deliberately narrow opcode.

## Highlights

- **`TNEW`**: an empty `{}` in a traced loop can allocate through
  `luaH_new` and publish the table to its stack slot.  Requires
  `jit.opt.start("ir=1","alloc=1")`.
- The table is allocated without a `luaC_checkGC` inside the trace; the
  existing rooted `L->top` protocol keeps it visible to the next GC.
- Only empty `NEWTABLE` is accepted, only in full root traces.  `SETLIST`,
  table writes to the new table, strings/closures, `ffi.new`, side traces,
  sinking and exact allocation counts are explicit non-goals.
- `tests/jit/74_trace_tnew.lua` covers opt-in/opt-out and a post-loop GC.

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

## Known limitations

- Allocated tables are not sunk; a loop allocating one table per iteration
  retains them until GC.
- Allocation-capable GC safepoints inside a trace remain unsupported; GC is
  deferred to the interpreter boundary.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
