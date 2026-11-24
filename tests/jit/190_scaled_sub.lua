-- jit/190_scaled_sub.lua -- native `s = s - a[i] * k` and
-- `s = s - a[i] / k` reductions.
--
-- The scaled-sum intrinsic only accepted an ADD into the accumulator, so
-- the subtract spelling stayed interpreted (or matched the wrong shape).
-- The product is computed first, then subtracted from the accumulator with
-- Lua's int-wrap/float-promotion rules.  `product - s` is a different
-- expression and must not be mistaken for this reduction.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/190_scaled_sub: skipped (no native target)")
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
jit.on()

local N = 400
local UP = {}
local FLT = {}
for i = 1, N do
  UP[i] = i * 3 - 7
  FLT[i] = i * 0.5 + 0.25
end
local K = 5

local function run (name, f, a, ...)
  jit.flush()
  jit.off()
  local want = { pcall(f, a, ...) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, a, ...) }
  local used = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.check(same(got[i], want[i]),
              name .. ": result " .. i .. " differs: " ..
              tostring(got[i]) .. " ~= " .. tostring(want[i]))
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  H.check(used > 0, name .. ": intrinsic did not run")
  return got
end

local function mul_const (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] * 3 end
  return s
end

local function div_const (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] / 3 end
  return s
end

local function mul_float_acc (a, n)
  local s = 0.0
  for i = 1, n do s = s - a[i] * 2.5 end
  return s
end

local function div_float_acc (a, n)
  local s = 0.0
  for i = 1, n do s = s - a[i] / 2.5 end
  return s
end

local function mul_reg (a, n, k)
  local s = 0
  local m = k
  for i = 1, n do s = s - a[i] * m end
  return s
end

local function div_reg (a, n, k)
  local s = 0
  local d = k
  for i = 1, n do s = s - a[i] / d end
  return s
end

local function mul_up_scalar (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] * K end
  return s
end

local function div_up_scalar (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] / K end
  return s
end

local function mul_up_table (a, n)
  local s = 0
  for i = 1, n do s = s - UP[i] * 2 end
  return s
end

local function mul_neg (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] * -3 end
  return s
end

local function div_neg (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] / -3 end
  return s
end

local function div_zero (a, n)
  local s = 0
  for i = 1, n do s = s - a[i] / 0 end
  return s
end

local function mul_down (a, n)
  local s = 0
  for i = n, 1, -1 do s = s - a[i] * 4 end
  return s
end

local function div_step (a, n)
  local s = 0
  for i = 1, n, 3 do s = s - a[i] / 4 end
  return s
end

local function observe (a, n)
  local s = 0
  local x, p
  for i = 1, n do
    x = a[i]
    p = x * 2
    s = s - p
  end
  return s, x, p
end

local got = run("multiply subtract", mul_const, UP, N)
run("divide subtract", div_const, UP, N)
run("multiply float accumulator", mul_float_acc, FLT, N)
run("divide float accumulator", div_float_acc, FLT, N)
run("register multiplier", mul_reg, UP, N, 4)
run("register divisor", div_reg, UP, N, 4)
run("upvalue multiplier", mul_up_scalar, UP, N)
run("upvalue divisor", div_up_scalar, UP, N)
run("upvalue table", mul_up_table, UP, N)
run("negative multiplier", mul_neg, UP, N)
run("negative divisor", div_neg, UP, N)
run("zero divisor", div_zero, FLT, N)
run("descending multiply", mul_down, UP, N)
run("strided divide", div_step, UP, N)
run("observed temporaries", observe, UP, N)

-- `product - s` is not the reduction: it must stay correct and must not
-- install the subtract intrinsic.
local function reversed (a, n)
  local s = 0
  for i = 1, n do s = a[i] * 2 - s end
  return s
end
jit.flush()
jit.off()
local rwant = reversed(UP, N)
jit.flush()
jit.on()
local before = jit.stats().tint_hits
local rgot = reversed(UP, N)
H.check(same(rgot, rwant), "reversed subtraction result")
H.eq(jit.stats().tint_hits, before, "reversed subtraction must not use the intrinsic")

-- Non-numeric elements still raise through the interpreter.
local bad = {1, 2, 3, "x"}
jit.flush()
jit.on()
local ok = pcall(function ()
  local s = 0
  for i = 1, 4 do s = s - bad[i] * 2 end
  return s
end)
H.check(not ok, "non-numeric element must raise")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/190_scaled_sub ok")
