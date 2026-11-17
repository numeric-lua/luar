# ADR 0002 — libffi for FFI calls, opt-in at build time

Date: 2026-11-17
Status: accepted (implementation in milestone 0.2)

## Context

LuaJIT calls C functions through hand-written assembly per architecture. That
is fast and dependency-free but is a large, platform-specific body of code
and a source of ABI bugs. LuaR's FFI must work on x86-64 Linux first, then
ARM64/macOS, without blocking on a hand-written call layer.

## Decision

Use **libffi** for fixed-argument C calls in the first FFI implementation.
The FFI is opt-in at build time (`WITH_FFI=1`), needs the libffi headers to
compile, and loads the shared library with `dlopen`/`dlsym` when the module
opens, so binaries and embedders carry no link-time libffi dependency.  It
reports availability through `luar.features.ffi`. The build without FFI
stays dependency-free.

The call layer is isolated behind `src/luar_ffi_call.c` so a future direct
call emitter (and the JIT's inline C calls) can replace libffi for common
signatures without touching the parser or cdata runtime.

## Consequences

Positive:

* correct ABI handling for integers, floats, pointers and struct-by-value
  on every libffi-supported platform;
* much smaller initial implementation and test surface;
* FFI remains optional for embedders who do not want the dependency.

Negative:

* a small per-call overhead compared to LuaJIT's direct stubs (expected to
  matter only for very chatty bindings, and only until milestone 0.7);
* one more build dependency (headers) plus a runtime shared library for FFI
  builds, and a clear error if libffi cannot be found at run time.

## Alternatives rejected

* **Hand-written SysV x86-64 stubs only** — fastest path to x86-64, but
  re-implements ABI corner cases (struct classification, varargs) and does
  not generalise; good later as an optimisation, bad as a foundation.
* **dyncall** — not installed and less ubiquitous than libffi.
* **Bundle libffi** — vendoring a large dependency for a 0.x milestone is
  worse than requiring the system copy.
