# Vendored upstream reference

`vendor/lua-5.5.1/` is the **unmodified** official Lua 5.5.1 source tree. It
is never compiled and never shipped: it exists so that `src/` can be compared
against the exact upstream release by `tools/upstream-check.sh` and
`tools/upstream-sync.sh`.

Because it is pristine, it still contains upstream's compile-time
`LUA_COMPAT_*` switches and the code paths they guard. LuaR's build removes
all of them: `src/` has no legacy Lua compatibility and targets Lua 5.5
only. Do not edit this directory; the sync policy is in `doc/upstream.md`.
