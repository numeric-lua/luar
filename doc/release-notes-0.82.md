# Luar 0.82 release notes

This release records the completed incremental J4/FFI tier in the roadmap.

## Highlights

- `doc/roadmap.md` now lists the 0.50–0.81 native-call, allocation/GC and
  performance increments with their exit criteria.
- Remaining work is explicitly separated: `B == 0`, varargs/aggregates,
  strings/closures/`ffi.new` allocation and sinking require the
  allocation-safepoint design.
- No runtime behavior change.

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
