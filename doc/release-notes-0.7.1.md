# Luar 0.7.1 release notes

This is a branding release.  The runtime, bytecode, C API, module ABI,
executables and `LUAR_*` identifiers are unchanged.

- Display name: **Luar** (previously “LuaR”).
- Version banner: `Luar 0.7.1 (Lua 5.5.1)`.
- Updated README, man page, documentation, release notes and CI banner checks.
- `luar`, `luarc`, `libluar`, `luar_*` C symbols and all `LUAR_*` macros stay
  exactly as before.

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
