-- jit/168_scaled_array_sum.lua -- native `s = s + a[i] * k` reductions.
--
-- The weighted-sum shape has a numeric constant, register scalar or
-- closure-upvalue scalar.  It was not covered by the two-array product
-- intrinsic, so it stayed interpreted.  The intrinsic keeps the last
-- element and product registers observable after the loop and follows the
-- interpreter's int-wrap / float-promotion order.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/168_scaled_array_sum: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")

local M = 3
local UP = {}
for i = 1, 400 do UP[i] = i end

local INT = {}
local FLT = {}
local MIX = {}
for i = 1, 400 do
  INT[i] = i * 5 - 7
  FLT[i] = i + 0.25
  MIX[i] = (i % 11 == 0) and (i - 0.5) or (i * 3)
end

local BIG = {}
for i = 1, 400 do BIG[i] = math.maxinteger - i end

local function run (f, a, n)
  jit.flush()
  jit.off()
  local want = { pcall(f, a, n) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, a, n) }
  local st = jit.stats()
  H.eq(#got, #want, "result count")
  H.eq(got[1], want[1], "error status")
  if got[1] then
    for i = 2, #want do
      H.check(same(got[i], want[i]),
              "result " .. i .. " differs: " .. tostring(got[i]) ..
              " ~= " .. tostring(want[i]))
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), "error message")
  end
  return st, before
end

local function scale_const (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] * 3 end
  return s
end

local function scale_const_float (a, n)
  local s = 0.5
  for i = 1, n do s = s + a[i] * 0.25 end
  return s
end

local function scale_const_neg (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] * -7 end
  return s
end

local function scale_reg (a, n)
  local m = M
  local s = 0
  for i = 1, n do s = s + a[i] * m end
  return s
end

local function scale_reg_left (a, n)
  local m = M
  local s = 0
  for i = 1, n do s = s + m * a[i] end
  return s
end

local function scale_up_scalar (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] * M end
  return s
end

local function scale_observe (a, n)
  local s = 0
  local x, p
  for i = 1, n do
    x = a[i]
    p = x * 3
    s = s + p
  end
  return s, x, p
end

local function scale_mixed (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] * 2.5 end
  return s
end

local function scale_up_table (a, n)
  local s = 0
  for i = 1, n do s = s + UP[i] * 3 end
  return s
end

local st, before = run(scale_const, INT, 400)
H.check(st.tint_hits > before, "constant scaled sum did not use the intrinsic")
H.check(st.compiled >= 1, "constant scaled sum did not compile")

st, before = run(scale_const_float, FLT, 400)
H.check(st.tint_hits > before, "float scaled sum did not use the intrinsic")
st, before = run(scale_const_neg, INT, 400)
H.check(st.tint_hits > before, "negative scaled sum did not use the intrinsic")
st, before = run(scale_reg, INT, 400)
H.check(st.tint_hits > before, "register scalar did not use the intrinsic")
st, before = run(scale_reg_left, INT, 400)
H.check(st.tint_hits > before, "left-hand register scalar did not use the intrinsic")
st, before = run(scale_up_scalar, INT, 400)
H.check(st.tint_hits > before, "upvalue scalar did not use the intrinsic")
st, before = run(scale_up_table, INT, 400)
H.check(st.tint_hits > before, "upvalue table did not use the intrinsic")
st, before = run(scale_observe, INT, 400)
H.check(st.tint_hits > before, "observed element/product did not use the intrinsic")

-- Mixed elements promote the accumulator exactly once and wrap exactly.
run(scale_mixed, MIX, 400)
run(scale_const, BIG, 400)
run(scale_const_neg, BIG, 400)
run(scale_const, INT, 400)

-- The upvalue table is re-resolved on every entry.
local saved = UP
UP = MIX
run(scale_up_table, INT, 400)
UP = saved

-- Fallbacks preserve the interpreter result and error.
local short = {}
for i = 1, 399 do short[i] = i end
run(scale_const, short, 400)

local meta = setmetatable({}, { __index = function (_, k) return k end })
for i = 1, 400 do rawset(meta, i, i) end
run(scale_const, meta, 400)

local bad = {}
for i = 1, 400 do bad[i] = i end
bad[200] = "not a number"
run(scale_const, bad, 400)

-- Non-unit steps stay on the interpreter for now.
local function scale_stepped (a, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] * 3 end
  return s
end
run(scale_stepped, INT, 400)

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "table=0", "tint=0")
print("jit/168_scaled_array_sum ok")
