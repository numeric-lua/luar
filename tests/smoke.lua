-- tests/smoke.lua -- fastest possible end-to-end check.

local function check (cond, msg)
  if not cond then error(msg or "smoke check failed", 2) end
end

-- identity
check(type(luar) == "table", "missing luar table")
check(luar.version == "0.165.0", "unexpected luar.version: " .. tostring(luar.version))
check(luar.lua_version == "Lua 5.5.1", "unexpected Lua base: " .. tostring(luar.lua_version))
check(_VERSION == "Lua 5.5", "unexpected _VERSION: " .. tostring(_VERSION))
check(luar.arch == "x64" or luar.arch == "arm64", "unknown arch " .. tostring(luar.arch))

-- C API identity
check(luar.version and luar.copyright, "identity strings missing")

-- jit surface
check(type(jit) == "table", "missing jit table")
check(type(jit.status) == "function", "missing jit.status")
local on = jit.status()
check(type(on) == "boolean", "jit.status must return a boolean")
check(jit.version_num == luar.version_num, "version mismatch")

-- table extensions
local t = table.new(4, 4)
t.a, t[1] = 1, 2
table.clear(t)
check(next(t) == nil, "table.clear left entries")

-- 5.5 native bitwise operators, and no legacy bit library
check(0xff & 0x0f == 15, "native bitwise and")
check(~0 == -1, "native bitwise not")
check(rawget(_G, "bit") == nil, "legacy 'bit' library must not exist")

-- 5.5 language bits
check(table.create ~= nil, "table.create missing (Lua 5.5)")

print("smoke ok")
