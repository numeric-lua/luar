-- tests/smoke.lua -- fastest possible end-to-end check.

local function check (cond, msg)
  if not cond then error(msg or "smoke check failed", 2) end
end

-- identity
check(type(luar) == "table", "missing luar table")
check(luar.version == "0.1.0", "unexpected luar.version: " .. tostring(luar.version))
check(luar.lua_version == "Lua 5.5.1", "unexpected Lua base: " .. tostring(luar.lua_version))
check(_VERSION == "Lua 5.5", "unexpected _VERSION: " .. tostring(_VERSION))
check(luar.arch == "x64" or luar.arch == "arm64", "unknown arch " .. tostring(luar.arch))

-- C API identity
check(luar.version and luar.copyright, "identity strings missing")

-- jit surface
check(type(jit) == "table", "missing jit table")
check(type(jit.status) == "function", "missing jit.status")
local on = jit.status()
check(on == false, "JIT should report off in 0.1")
check(jit.version_num == luar.version_num, "version mismatch")

-- table extensions
local t = table.new(4, 4)
t.a, t[1] = 1, 2
table.clear(t)
check(next(t) == nil, "table.clear left entries")

-- bit compatibility
check(bit.band(0xff, 0x0f) == 15, "bit.band")
check(bit.tohex(-1) == "ffffffff", "bit.tohex")
check(bit.bxor(0xff, 0x0f) == 0xf0, "bit.bxor")

-- 5.5 language bits
check(table.create ~= nil, "table.create missing (Lua 5.5)")

print("smoke ok")
