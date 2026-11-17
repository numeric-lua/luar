-- Minimal stand-in for LuaJIT's 32-bit `bit` module, needed by
-- vendor/luajit/dynasm/dasm_x86.lua when the host is stock Lua 5.5.
-- Only the four functions DynASM calls are provided.
local M = {}
local function u32(x) return x & 0xffffffff end
function M.band(a, b) return u32(a) & u32(b) end
function M.bxor(a, b) return u32(a) ~ u32(b) end
function M.lshift(a, n) return u32(u32(a) << (n & 31)) end
function M.rshift(a, n) return u32(a) >> (n & 31) end
return M
