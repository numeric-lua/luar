-- compat/extensions.lua -- the Luar / LuaJIT-compatible extension surface.

local H = dofile("tests/helpers.lua")

-- identity table
H.eq(type(luar), "table")
H.eq(luar.version, "0.45.0")
H.eq(luar.lua_version, "Lua 5.5.1")
H.check(luar.os == "Linux" or luar.os == "OSX", "unexpected os " .. tostring(luar.os))
H.check(luar.arch == "x64" or luar.arch == "arm64",
        "unexpected arch " .. tostring(luar.arch))
H.eq(luar.features.table, true)

-- require() and globals agree
H.eq(require("luar"), luar)
H.eq(require("jit"), jit)
H.eq(rawget(_G, "bit"), nil, "no legacy bit library")

-- module paths point at the luar directory first
H.check(package.path:find("/luar/5.5/", 1, true), package.path)
H.check(package.cpath:find("/luar/5.5/", 1, true), package.cpath)

-- jit control surface (truthful: no engine yet)
H.eq(type(jit), "table")
H.eq(type(jit.status), "function")
H.check(type(jit.status()) == "boolean")
H.eq(jit.on(), nil)
H.eq(jit.off(), nil)
H.eq(jit.flush(), nil)
H.eq(jit.opt.start("hotloop=56", "maxtrace=1000"), nil)
H.eq(jit.os, luar.os)
H.eq(jit.arch, luar.arch)
H.eq(jit.version, "Luar 0.45.0")

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

-- 5.5 native bitwise operators replace the 5.1-era bit library
H.eq(0xff & 0x0f, 15)
H.eq(0x80000000 | 0, 0x80000000)
H.eq(~0, -1)
H.eq(-1 >> 63, 1)

print("compat/extensions ok")
