-- compat/strict55.lua -- Luar is Lua 5.5 only: no legacy compatibility.
--
-- This suite is Luar-specific (stock Lua 5.5 may be built with
-- LUA_COMPAT_GLOBAL and would behave differently for 'global').

local H = dofile("tests/helpers.lua")

-- 'global' is always a reserved word
H.check(load("local global = 1") == nil, "'local global' must be a syntax error")
H.check(load("global = 1") == nil, "'global = 1' must be a syntax error")
H.check(load("global x = 1; return x") ~= nil, "global declarations must parse")

-- there is no 5.1-era bit library
H.eq(rawget(_G, "bit"), nil)
H.eq(package.loaded["bit"], nil)
H.check(not pcall(require, "bit"), "require 'bit' must fail")
H.eq(luar.features.bit, nil)

-- deprecated math functions are absent
for _, name in ipairs{ "pow", "atan2", "log10", "sinh", "cosh", "tanh" } do
  H.eq(math[name], nil, "math." .. name .. " must not exist")
end
H.eq(math.mod, nil)

-- removed global functions and library names are absent
for _, name in ipairs{
  "unpack", "loadstring", "module", "setfenv", "getfenv", "newproxy",
} do
  H.eq(rawget(_G, name), nil, name .. " must not exist")
end
H.eq(string.gfind, nil)
H.eq(table.maxn, nil)

-- native operators replace the bit library
H.eq(7 & 3, 3)
H.eq(1 << 63, math.mininteger)

-- the feature matrix advertises only what exists
H.eq(type(luar.features), "table")
H.eq(luar.features.table, true)
H.eq(type(luar.features.jit), "boolean")
H.eq(type(luar.features.ffi), "boolean")

print("compat/strict55 ok")
