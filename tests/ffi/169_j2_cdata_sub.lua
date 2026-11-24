-- ffi/169_j2_cdata_sub.lua -- J2 single-source subtraction over bounded
-- cdata arrays: s = s - a[i], including negative steps and mixed types.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/169_j2_cdata_sub: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
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

local n = 160
local ai = ffi.new("int32_t[?]", n)
local ad = ffi.new("double[?]", n)
for i = 0, n - 1 do
  ai[i] = i * 5 - 300
  ad[i] = i * 0.5 + 0.25
end

-- Register array, integer accumulator, ascending step.
local f1 = assert(load([[
  return function (a, n)
    local s = 0
    for i = 0, n - 1 do s = s - a[i] end
    return s
  end
]], "=csub1"))()
local before = jit.stats().tint_hits
local got = f1(ai, n)
local used = jit.stats().tint_hits - before
local want = 0
for i = 0, n - 1 do want = want - ai[i] end
H.check(same(got, want), "int32 subtraction")
H.check(used > 0, "cdata subtraction intrinsic did not run")

-- Float accumulator over an integer array (promotion).
local f2 = assert(load([[
  return function (a, n)
    local s = 0.0
    for i = 0, n - 1 do s = s - a[i] end
    return s
  end
]], "=csub2"))()
before = jit.stats().tint_hits
got = f2(ai, n)
used = jit.stats().tint_hits - before
want = 0.0
for i = 0, n - 1 do want = want - ai[i] end
H.check(same(got, want), "float accumulator subtraction")
H.check(used > 0, "mixed cdata subtraction intrinsic did not run")

-- Descending step and a double array captured as a closure upvalue.  The
-- upvalue must be bound by the surrounding function, not by load().
local a = ad
local function f3 (n)
  local s = 0
  for i = n - 1, 0, -1 do s = s - a[i] end
  return s
end
before = jit.stats().tint_hits
got = f3(n)
used = jit.stats().tint_hits - before
want = 0
for i = n - 1, 0, -1 do want = want - ad[i] end
H.check(same(got, want), "descending double subtraction")
H.check(used > 0, "upvalue cdata subtraction intrinsic did not run")

jit.opt.start("ir=0", "hotloop=56", "hotexit=10", "tint=1")
jit.flush()
print("ffi/169_j2_cdata_sub ok")
