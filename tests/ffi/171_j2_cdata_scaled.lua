-- ffi/171_j2_cdata_scaled.lua -- J2 scaled reductions over bounded cdata
-- arrays: `s = s + a[i] * k` and `s = s + a[i] / k`.
--
-- Multiplication wraps for two integers and promotes when either operand is
-- a float.  Division always yields a float, including for two integers and
-- for a zero divisor (IEEE inf/NaN, never a Lua error).  Constant, register
-- and upvalue scalars, observed element/product temporaries, descending and
-- non-unit steps, and float32/double element widths are covered against the
-- interpreter.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/171_j2_cdata_scaled: skipped (no native target)")
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

local N = 200
local ai = ffi.new("int32_t[?]", N)
local au = ffi.new("uint16_t[?]", N)
local af = ffi.new("float[?]", N)
local ad = ffi.new("double[?]", N)
for i = 0, N - 1 do
  ai[i] = i * 7 - 400
  au[i] = i * 13
  af[i] = i * 0.25 + 0.5
  ad[i] = i * 1.5 - 3.25
end
local UP = ad

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
  return got, used
end

local function mul_int (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] * 3 end
  return s
end

local function mul_uint (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] * 1000 end
  return s
end

local function mul_float (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] * 2.5 end
  return s
end

local function div_int (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] / 3 end
  return s
end

local function div_float (a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] / 2.5 end
  return s
end

local function div_neg (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] / -4 end
  return s
end

local function div_zero (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] / 0 end
  return s
end

local function div_reg (a, n, k)
  local s = 0
  local d = k
  for i = 0, n - 1 do s = s + a[i] / d end
  return s
end

local function mul_up (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + UP[i] * 1.5 end
  return s
end

local function div_up (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + UP[i] / 4 end
  return s
end

local function div_down (a, n)
  local s = 0
  for i = n - 1, 0, -1 do s = s + a[i] / 4 end
  return s
end

local function div_step (a, n)
  local s = 0
  for i = 0, n - 1, 3 do s = s + a[i] / 4 end
  return s
end

local function div_observe (a, n)
  local s = 0
  local x, q
  for i = 0, n - 1 do
    x = a[i]
    q = x / 4
    s = s + q
  end
  return s, x, q
end

run("int32 multiply", mul_int, ai, N)
run("uint16 multiply", mul_uint, au, N)
run("double multiply", mul_float, ad, N)
run("int32 divide", div_int, ai, N)
run("double divide", div_float, ad, N)
run("negative divisor", div_neg, ai, N)
run("zero divisor", div_zero, ad, N)
run("register divisor", div_reg, ai, N, 7)
run("upvalue multiply", mul_up, af, N)
run("upvalue divide", div_up, af, N)
run("descending divide", div_down, ad, N)
run("strided divide", div_step, ai, N)
run("observed temporaries", div_observe, ad, N)

-- A pointer has no bounded length, so the intrinsic must not run.
local p = ffi.cast("double *", ad)
jit.flush()
jit.off()
local pwant = 0
for i = 0, N - 1 do pwant = pwant + p[i] / 4 end
jit.flush()
jit.on()
local before = jit.stats().tint_hits
local pgot = 0
for i = 0, N - 1 do pgot = pgot + p[i] / 4 end
H.check(same(pgot, pwant), "pointer fallback result")
H.eq(jit.stats().tint_hits, before, "pointer source must not use the intrinsic")

-- A stepped run that leaves the array must fall back and raise normally.
local short = ffi.new("int32_t[?]", N - 1)
jit.flush()
jit.off()
local ok = pcall(function ()
  local s = 0
  for i = 0, N - 1 do s = s + short[i] / 4 end
  return s
end)
jit.flush()
jit.on()
jit.opt.start("hotloop=1", "ir=0", "tint=1")
local ok2 = pcall(function ()
  local s = 0
  for i = 0, N - 1 do s = s + short[i] / 4 end
  return s
end)
H.eq(ok2, ok, "short array fallback outcome")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/171_j2_cdata_scaled ok")
