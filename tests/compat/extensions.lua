-- compat/extensions.lua -- the LuaR / LuaJIT-compatible extension surface.

local H = dofile("tests/helpers.lua")

-- identity table
H.eq(type(luar), "table")
H.eq(luar.version, "0.1.0")
H.eq(luar.lua_version, "Lua 5.5.1")
H.eq(luar.os, "Linux")
H.eq(luar.arch, "x64")
H.eq(luar.features.bit, true)
H.eq(luar.features.table, true)

-- require() and globals agree
H.eq(require("luar"), luar)
H.eq(require("jit"), jit)
H.eq(require("bit"), bit)

-- module paths point at the luar directory first
H.check(package.path:find("/luar/5.5/", 1, true), package.path)
H.check(package.cpath:find("/luar/5.5/", 1, true), package.cpath)

-- jit control surface (truthful: no engine yet)
H.eq(type(jit), "table")
H.eq(type(jit.status), "function")
H.eq(jit.status(), false)
H.eq(jit.on(), nil)
H.eq(jit.off(), nil)
H.eq(jit.flush(), nil)
H.eq(jit.opt.start("hotloop=56", "maxtrace=1000"), nil)
H.eq(jit.os, luar.os)
H.eq(jit.arch, luar.arch)
H.eq(jit.version, "LuaR 0.1.0")

-- table extensions
local t = table.new(8, 8)
H.eq(type(t), "table")
H.eq(#t, 0)
t[1], t.key = "v", 2
H.eq(t[1], "v")
H.eq(t.key, 2)
table.clear(t)
H.eq(next(t), nil)

-- table.new defaults / negative checks
H.eq(type(table.new()), "table")
H.raises(function() table.clear(42) end, "table expected")

-- bit library (LuaJIT 32-bit semantics)
H.eq(bit.tobit(0xffffffff), -1)
H.eq(bit.band(0xff, 0x0f), 15)
H.raises(function() bit.band() end, "number expected", "zero-arg band")
H.eq(bit.tobit(1.5), 2)               -- LuaJIT rounds to nearest even
H.eq(bit.tobit(3.7), 4)
H.eq(bit.tobit(-3.7), -4)
H.eq(bit.bxor(0xff, 0x0f), 0xf0)
H.eq(bit.bnot(0), -1)
H.eq(bit.lshift(1, 31), -2147483648)
H.eq(bit.rshift(-1, 31), 1)
H.eq(bit.arshift(-2, 1), -1)
H.eq(bit.rol(0x12345678, 8), 0x34567812)
H.eq(bit.ror(0x12345678, 8), 0x78123456)
H.eq(bit.bswap(0x12345678), 0x78563412)
H.eq(bit.tohex(0xdeadbeef), "deadbeef")
H.eq(bit.tohex(0xdeadbeef, 4), "beef")
H.eq(bit.tohex(0xdeadbeef, -4), "BEEF")
H.eq(bit.nbits, 32)

print("compat/extensions ok")
