-- jit/188_scaled_div.lua -- native `s = s + a[i] / k` reductions.
--
-- The scaled-sum intrinsic handled multiplication only.  Division always
-- produces a float in Lua, including for two integers, and must preserve
-- the interpreter's evaluation order: divide the element first, then add
-- the quotient to the accumulator.  Constant, register and upvalue
-- scalars, observed temporaries, mixed numeric elements, negative and
-- zero divisors and non-unit steps are covered.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/188_scaled_div: skipped (no native target)")
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
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")

local N = 400
local UP = {}
local FLT = {}
local MIX = {}
for i = 1, N do
  UP[i] = i
  FLT[i] = i + 0.25
  MIX[i] = (i % 13 == 0) and (i - 0.5) or (i * 2)
end

local function run (f, a, n, ...)
  jit.flush()
  jit.off()
  local want = { pcall(f, a, n, ...) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, a, n, ...) }
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

local function div_const (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / 3 end
  return s
end

local function div_float_acc (a, n)
  local s = 0.0
  for i = 1, n do s = s + a[i] / 2.5 end
  return s
end

local function div_const_neg (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / -2 end
  return s
end

local function div_zero (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / 0 end
  return s
end

local function div_reg (a, n, k)
  local s = 0
  local d = k
  for i = 1, n do s = s + a[i] / d end
  return s
end

local D = 7
local function div_up_scalar (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / D end
  return s
end

local function div_float_const (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / 3.0 end
  return s
end

local function div_large (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / 0x1000000000000000 end
  return s
end

local function div_up_table (a, n)
  local s = 0
  for i = 1, n do s = s + UP[i] / 3 end
  return s
end

local function div_observe (a, n)
  local s = 0
  local x, q
  for i = 1, n do
    x = a[i]
    q = x / 4
    s = s + q
  end
  return s, x, q
end

local function div_mixed (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] / 1.5 end
  return s
end

local function div_down (a, n)
  local s = 0
  for i = n, 1, -1 do s = s + a[i] / 4 end
  return s
end

local function div_step (a, n)
  local s = 0
  for i = 1, n, 3 do s = s + a[i] / 4 end
  return s
end

local st, before = run(div_const, UP, N)
H.check(st.tint_hits > before, "constant divisor did not use the intrinsic")
H.check(st.compiled >= 1, "constant divisor did not compile")

st, before = run(div_float_acc, FLT, N)
H.check(st.tint_hits > before, "float accumulator did not use the intrinsic")
st, before = run(div_const_neg, UP, N)
H.check(st.tint_hits > before, "negative divisor did not use the intrinsic")
st, before = run(div_zero, UP, N)
H.check(st.tint_hits > before, "zero divisor did not use the intrinsic")
st, before = run(div_reg, UP, N, 7)
H.check(st.tint_hits > before, "register divisor did not use the intrinsic")
st, before = run(div_up_scalar, UP, N)
H.check(st.tint_hits > before, "upvalue scalar did not use the intrinsic")
st, before = run(div_float_const, UP, N)
H.check(st.tint_hits > before, "float constant divisor did not use the intrinsic")
st, before = run(div_large, UP, N)
H.check(st.tint_hits > before, "large integer divisor did not use the intrinsic")
D = -4
st, before = run(div_up_scalar, UP, N)
H.check(st.tint_hits > before, "rebound upvalue divisor did not use the intrinsic")
D = 7

st, before = run(div_up_table, UP, N)
H.check(st.tint_hits > before, "upvalue table did not use the intrinsic")
st, before = run(div_observe, UP, N)
H.check(st.tint_hits > before, "observed quotient did not use the intrinsic")
run(div_mixed, MIX, N)
run(div_down, UP, N)
run(div_step, UP, N)

-- A metatable or a missing array slot must fall back without running the
-- intrinsic.  (A missing slot raises in the interpreter too.)
local meta = setmetatable({}, { __index = function (_, k) return k end })
for i = 1, N do rawset(meta, i, i) end
local mf = assert(load([[
  return function (a, n)
    local s = 0
    for i = 1, n do s = s + a[i] / 3 end
    return s
  end
]], "=sdiv_meta"))()
st, before = run(mf, meta, N)
H.eq(st.tint_hits, before, "metatable source must not use the intrinsic")

local short = {}
for i = 1, N - 1 do short[i] = i end
st, before = run(div_const, short, N)
H.eq(st.tint_hits, before, "short array must not use the intrinsic")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/188_scaled_div ok")
