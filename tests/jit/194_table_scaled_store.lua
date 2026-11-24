-- jit/194_table_scaled_store.lua -- native `dst[i] = src[i] OP k`
-- stores over raw table arrays.
--
-- The default tier had scaled cdata stores but no equivalent for raw Lua
-- tables.  Add/subtract/multiply/divide with constant, register and
-- closure-upvalue scalars now run in one C pass, including in-place
-- stores, destination growth and preserved hash entries.  Metatables and
-- non-numeric source elements stay on the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/194_table_scaled_store: skipped (no native target)")
  return
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return string.pack("d", a) == string.pack("d", b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 200
local S = {}
for i = 1, N do S[i] = i * 3 - 7 end
local K = 4
local US = S

local function run (name, f, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local wt = f({}, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local gt = f({}, src, ...)
  local used = jit.stats().tint_hits - before
  H.check(used > 0, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(gt[i], wt[i]),
            name .. ": element " .. i .. " differs: " ..
            tostring(gt[i]) .. " ~= " .. tostring(wt[i]))
    H.eq(math.type(gt[i]), math.type(wt[i]), name .. ": type " .. i)
  end
  return used
end

local function add_const (dst, src, n)
  for i = 1, n do dst[i] = src[i] + 3 end
  return dst
end

local function sub_const (dst, src, n)
  for i = 1, n do dst[i] = src[i] - 3 end
  return dst
end

local function mul_const (dst, src, n)
  for i = 1, n do dst[i] = src[i] * 2.5 end
  return dst
end

local function div_const (dst, src, n)
  for i = 1, n do dst[i] = src[i] / 4 end
  return dst
end

local function add_reg (dst, src, n, k)
  local v = k
  for i = 1, n do dst[i] = src[i] + v end
  return dst
end

local function mul_reg (dst, src, n, k)
  local v = k
  for i = 1, n do dst[i] = src[i] * v end
  return dst
end

local function div_reg (dst, src, n, k)
  local v = k
  for i = 1, n do dst[i] = src[i] / v end
  return dst
end

local function mul_up_scalar (dst, src, n)
  for i = 1, n do dst[i] = src[i] * K end
  return dst
end

local function div_up_scalar (dst, src, n)
  for i = 1, n do dst[i] = src[i] / K end
  return dst
end

local function mul_up_src (dst, src, n)
  for i = 1, n do dst[i] = US[i] * 3 end
  return dst
end

local function div_zero (dst, src, n)
  for i = 1, n do dst[i] = src[i] / 0 end
  return dst
end

local function down (dst, src, n)
  for i = n, 1, -1 do dst[i] = src[i] * 2 end
  return dst
end

local function step (dst, src, n)
  for i = 1, n, 3 do dst[i] = src[i] / 2 end
  return dst
end

run("add constant", add_const, S, N)
run("subtract constant", sub_const, S, N)
run("multiply constant", mul_const, S, N)
run("divide constant", div_const, S, N)
run("add register", add_reg, S, N, 9)
run("multiply register", mul_reg, S, N, 7)
run("divide register", div_reg, S, N, 7)
run("multiply upvalue scalar", mul_up_scalar, S, N)
run("divide upvalue scalar", div_up_scalar, S, N)
run("upvalue source", mul_up_src, S, N)
run("zero divisor", div_zero, S, N)
run("descending", down, S, N)
run("strided", step, S, N)

-- The value register keeps the last computed value.
local function observe (dst, src, n)
  local x
  for i = 1, n do
    x = src[i] * 3
    dst[i] = x
  end
  return dst, x
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local wd, wx = observe({}, S, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
local gd, gx = observe({}, S, N)
H.check(jit.stats().tint_hits > before, "observed store did not run")
H.check(same(gx, wx), "observed value register")
for i = 1, N do H.check(same(gd[i], wd[i]), "observed element " .. i) end

-- In-place store: destination and source are the same table.
local function inplace (a, n)
  for i = 1, n do a[i] = a[i] * 2 end
  return a
end
local base1, base2 = {}, {}
for i = 1, N do base1[i] = S[i]; base2[i] = S[i] end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(base1, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
inplace(base2, N)
H.check(jit.stats().tint_hits > before, "in-place store did not run")
for i = 1, N do H.check(same(base2[i], base1[i]), "in-place element " .. i) end

-- Destination growth preserves existing hash and later array keys.
local function grow (dst, src, n)
  for i = 1, n do dst[i] = src[i] + 1 end
  return dst
end
local g1 = { marker = "keep", [N + 5] = 99 }
local g2 = { marker = "keep", [N + 5] = 99 }
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
grow(g1, S, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
grow(g2, S, N)
H.eq(g2.marker, "keep", "hash key preserved")
H.eq(g2[N + 5], 99, "later array key preserved")
for i = 1, N do H.check(same(g2[i], g1[i]), "grown element " .. i) end

-- A destination metatable must keep __newindex in the interpreter.
local seen = 0
local meta = setmetatable({}, {
  __newindex = function (t, k, v) seen = seen + 1; rawset(t, k, v) end,
})
local function metaf (dst, src, n)
  for i = 1, n do dst[i] = src[i] * 2 end
  return dst
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
metaf(meta, S, N)
H.eq(jit.stats().tint_hits, before, "metatable destination must not use the intrinsic")
H.eq(seen, N, "__newindex call count")

-- A non-numeric source element must fall back and raise normally.
local bad = {}
for i = 1, N do bad[i] = S[i] end
bad[10] = "x"
local ok = pcall(function () return mul_const({}, bad, N) end)
H.check(not ok, "non-numeric source must raise")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/194_table_scaled_store ok")
