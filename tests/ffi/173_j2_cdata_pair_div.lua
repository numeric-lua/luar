-- ffi/173_j2_cdata_pair_div.lua -- J2 two-source cdata division:
-- `s = s + a[i] / b[i]`.
--
-- Division always produces a float and is evaluated left-to-right.  Both
-- arrays are bounded cdata sources; integer, unsigned, float and double
-- element combinations promote exactly like the interpreter, and a zero
-- denominator follows IEEE (inf/NaN), never a Lua error.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/173_j2_cdata_pair_div: skipped (no native target)")
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

local N = 180
local ai = ffi.new("int32_t[?]", N)
local au = ffi.new("uint16_t[?]", N)
local af = ffi.new("float[?]", N)
local ad = ffi.new("double[?]", N)
local bd = ffi.new("double[?]", N)
local bi = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do
  ai[i] = i * 7 - 300
  au[i] = i * 13
  af[i] = i * 0.25 + 0.5
  ad[i] = i * 1.5 - 2.25
  bi[i] = i + 1
  bd[i] = (i % 11 == 0) and 0.0 or (i * 0.5 + 1.25)
end
local UA, UB = ad, bd

local function run (name, f, a, b, ...)
  jit.flush()
  jit.off()
  local want = { pcall(f, a, b, ...) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, a, b, ...) }
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
end

local function div_int (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] / b[i] end
  return s
end

local function div_uint (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] / b[i] end
  return s
end

local function div_mixed (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] / b[i] end
  return s
end

local function div_float (a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] / b[i] end
  return s
end

local function div_up (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + UA[i] / UB[i] end
  return s
end

local function div_down (a, b, n)
  local s = 0
  for i = n - 1, 0, -1 do s = s + a[i] / b[i] end
  return s
end

local function div_step (a, b, n)
  local s = 0
  for i = 0, n - 1, 3 do s = s + a[i] / b[i] end
  return s
end

local function observe (a, b, n)
  local s = 0
  local x, y, q
  for i = 0, n - 1 do
    x = a[i]
    y = b[i]
    q = x / y
    s = s + q
  end
  return s, x, y, q
end

run("int32 pair division", div_int, ai, bi, N)
run("uint16 pair division", div_uint, au, bi, N)
run("float/double pair division", div_mixed, af, bd, N)
run("double pair division", div_float, ad, bd, N)
run("upvalue pair division", div_up, ad, bd, N)
run("descending pair division", div_down, ad, bd, N)
run("strided pair division", div_step, ai, bi, N)
run("observed temporaries", observe, ad, bd, N)

-- A pointer has no bounded length: the intrinsic must not run.
local p = ffi.cast("double *", ad)
jit.flush()
jit.off()
local pwant = 0
for i = 0, N - 1 do pwant = pwant + p[i] / bd[i] end
jit.flush()
jit.on()
local before = jit.stats().tint_hits
local pgot = 0
for i = 0, N - 1 do pgot = pgot + p[i] / bd[i] end
H.check(same(pgot, pwant), "pointer fallback result")
H.eq(jit.stats().tint_hits, before, "pointer source must not use the intrinsic")

-- A mixed table/cdata pair has no common reduction and stays interpreted.
local t = {}
for i = 0, N - 1 do t[i + 1] = ai[i] end
jit.flush()
jit.on()
local mixed_ok = pcall(function ()
  local s = 0
  for i = 1, N do s = s + t[i] / bi[i - 1] end
  return s
end)
H.check(mixed_ok, "table/cdata pair must run on the interpreter")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/173_j2_cdata_pair_div ok")
