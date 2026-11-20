# Luar 0.50.1 release notes

This is a test-portability fix for 0.50.0.

- `tests/ffi/25_trace_ccall0.lua` now skips when `jit.status()` is false, so
  the macOS FFI job no longer fails on a platform without the native target.
- No runtime behavior changed.
