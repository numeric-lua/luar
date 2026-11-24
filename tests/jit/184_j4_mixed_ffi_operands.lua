-- jit/184_j4_mixed_ffi_operands.lua -- J4 must not reinterpret a mixed
-- integer/float operand as an integer.
--
-- A trace that reads an integer cdata array into a float accumulator (or a
-- double array into an integer accumulator) used to be accepted because the
-- FFI operand-type check was bypassed.  The integer emitter then did integer
-- arithmetic on the double payload.  The runtime FADD/FSUB helpers already
-- perform Lua's mixed numeric conversion, so the trace is now accepted only
-- when every operand is numeric and those helpers do the conversion.  The
-- source slot must never be rewritten in place: it can be a visible local or
-- even a FORLOOP control slot.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/184_j4_mixed_ffi_operands: skipped (no native target)")
  return
end
local okffi, ffi = pcall(require, "ffi")
if not okffi then
  print("jit/184_j4_mixed_ffi_operands: skipped (FFI unavailable)")
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
local function loadf (src)
  return assert(load(src, "=j4mixed"))()
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1", "table=0", "tint=0")
jit.on()

local n = 180
local ai = ffi.new("int[?]", n)
local ad = ffi.new("double[?]", n)
for i = 0, n - 1 do
  ai[i] = i * 3 - 50
  ad[i] = i * 0.5 + 0.25
end

-- float accumulator over an integer cdata array
local f1 = loadf([[
  return function (a, n)
    local s = 0.0
    for i = 0, n - 1 do s = s + a[i] end
    return s
  end
]])
local before = jit.stats().ir_compiled
local got = f1(ai, n)
local want = 0.0
for i = 0, n - 1 do want = want + ai[i] end
H.check(same(got, want), "float accumulator over int[]")
H.check(jit.stats().ir_compiled > before, "mixed int[] trace did not compile")

-- integer accumulator over a double cdata array
local f2 = loadf([[
  return function (a, n)
    local s = 0
    for i = 0, n - 1 do s = s + a[i] end
    return s
  end
]])
before = jit.stats().ir_compiled
got = f2(ad, n)
want = 0
for i = 0, n - 1 do want = want + ad[i] end
H.check(same(got, want), "integer accumulator over double[]")
H.check(jit.stats().ir_compiled > before, "mixed double[] trace did not compile")

-- two mixed sources in one reduction
local f3 = loadf([[
  return function (a, b, n)
    local s = 0
    for i = 0, n - 1 do s = s + a[i] + b[i] end
    return s
  end
]])
before = jit.stats().ir_compiled
got = f3(ai, ad, n)
want = 0
for i = 0, n - 1 do want = want + ai[i] + ad[i] end
H.check(same(got, want), "mixed two-source reduction")
H.check(jit.stats().ir_compiled > before, "mixed two-source trace did not compile")

-- The loop index is a control slot; converting it in place would corrupt
-- the native FORLOOP.  The result must match the interpreter exactly.
local f4 = loadf([[
  return function (n)
    local t = 0.0
    local u = 0.0
    for i = n, 1, -1 do
      u = u + 1
      u = t + i
    end
    return u
  end
]])
H.eq(f4(10), 1.0, "mixed add must not rewrite the loop index")

-- A visible integer local must keep its value and subtype after a mixed add.
local f5 = loadf([[
  return function (n)
    local x = 9007199254740993
    local t = 0.0
    local u = 0.0
    for i = n, 1, -1 do u = t + x end
    return u, x, math.type(x)
  end
]])
local u, x, xt = f5(10)
H.check(same(u, 9007199254740992.0), "mixed add result")
H.eq(x, 9007199254740993, "mixed add rewrote a visible integer local")
H.eq(xt, "integer", "visible local changed subtype")

jit.flush()
jit.opt.start("ir=0", "hotloop=56", "hotexit=10", "table=1", "tint=1")
jit.on()
print("jit/184_j4_mixed_ffi_operands ok")
