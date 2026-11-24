-- jit/219_table_value_fill.lua -- fills with arbitrary non-numeric values.
--
-- `t[i] = "x"`, `t[i] = true`, `t[i] = obj`, `t[i] = U` and `U[i] = value`
-- now run through one luaH_setint loop per entry; numeric fills keep the
-- specialised affine helper.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/219_table_value_fill: skipped (no native target)")
  return
end

local N = 200
local function run (name, f, check)
  local t = {}
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(t, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do check(t, i, name) end
end

local function fill_str (t, n) for i = 1, n do t[i] = "x" end end
local function fill_true (t, n) for i = 1, n do t[i] = true end end
local function fill_nil (t, n) for i = 1, n do t[i] = nil end end
local function fill_tab (t, n) local v = { tag = 1 }; for i = 1, n do t[i] = v end end

run("string", fill_str, function (t, i) H.eq(t[i], "x", "string " .. i) end)
run("boolean", fill_true, function (t, i) H.eq(t[i], true, "boolean " .. i) end)
run("nil", fill_nil, function (t, i) H.eq(t[i], nil, "nil " .. i) end)
run("table", fill_tab, function (t, i)
  H.eq(type(t[i]), "table", "table type " .. i)
  H.eq(t[i].tag, 1, "table value " .. i)
end)

-- Upvalue destination.
local U = {}
local function fill_up (n) for i = 1, n do U[i] = "u" end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
fill_up(N)
H.check(jit.stats().tint_hits > before, "upvalue destination fill did not run")
for i = 1, N do H.eq(U[i], "u", "upvalue " .. i) end

-- Numeric fills still work (specialised affine path).
local function fill_num (t, n) for i = 1, n do t[i] = 7 end end
local t = {}
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
fill_num(t, N)
H.check(jit.stats().tint_hits > before, "numeric fill did not run")
for i = 1, N do H.eq(t[i], 7, "numeric " .. i) end

-- Metatable destination falls back.
local seen = 0
local m = setmetatable({}, { __newindex = function () seen = seen + 1 end })
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
fill_str(m, N)
H.eq(jit.stats().tint_hits, before, "metatable fill must not run")
H.eq(seen, N, "metatable handler calls")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/219_table_value_fill ok")
