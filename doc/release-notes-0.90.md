# Luar 0.90 release notes

Luar is a Lua 5.5.1 runtime with LuaJIT-style extensions.  This release
enables guarded short-string upvalues in traces.

## Highlights

- `TAG_STRING` guards short strings loaded via `GETUPVAL`; the verifier now
  accepts the tag.
- A string upvalue can be copied or concatenated inside a trace.
- Long strings fall outside the short-string tag and stay interpreter-side.
- `tests/jit/94_trace_string_upvalue.lua` covers copy, concat, forced GC and
  the long-string fallback.

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

- Only short strings use `TAG_STRING`; long strings fall back.
- Luar supports only Lua 5.5; no 5.1–5.4 compatibility mode.
