-- jit/183_j4_mixed_reduction.lua -- J4 compiles a loop that carries two
-- reductions at different numeric widths (integer and double).
--
-- The recorder used to classify a trace as integer-only whenever its
-- dynamic table reads had no explicit float op, or it rejected a mixed
-- trace at the first integer cdata read.  Both cases left an active and
-- common loop on the interpreter.  They are now handled by one trace while
-- pure-integer and pure-float traces keep their existing form.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/183_j4_mixed_reduction: skipped (no native target)")
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
jit.opt.start("ir=1", "hotloop=1", "hotexit=1", "table=0", "tint=0")
jit.on()

-- Mixed cdata arrays: s is an integer accumulator, x a double one.
local okffi, ffi = pcall(require, "ffi")
if okffi then
  local n = 600
  local ai = ffi.new("int[?]", n + 1)
  local ad = ffi.new("double[?]", n + 1)
  for i = 0, n do
    ai[i] = i * 3 - 100
    ad[i] = i * 0.5 + 0.25
  end
  local function mixed_cdata (count)
    local s, x = 0, 0.0
    for i = 0, count - 1 do
      s = s + ai[i]
      x = x + ad[i]
    end
    return s, x
  end
  local s1, x1 = mixed_cdata(n)
  local s2, x2 = mixed_cdata(n)
  local want_s, want_x = 0, 0.0
  for i = 0, n - 1 do
    want_s = want_s + ai[i]
    want_x = want_x + ad[i]
  end
  H.eq(s1, want_s, "mixed cdata integer accumulator")
  H.check(same(x1, want_x), "mixed cdata double accumulator")
  H.eq(s2, want_s, "mixed cdata second call")
  H.check(same(x2, want_x), "mixed cdata second double result")
  H.check(jit.stats().ir_compiled >= 1,
          "mixed cdata reduction did not compile in J4")
  H.check(jit.stats().ir_executed >= 1,
          "mixed cdata reduction never entered native code")
end

-- Mixed table arrays exercise the dynamic GETTABLE path.
local ti, tf = {}, {}
local tn = 500
for i = 1, tn do
  ti[i] = i * 2 - 50
  tf[i] = i * 0.25
end
local function mixed_tables (count)
  local s, x = 0, 0.0
  for i = 1, count do
    s = s + ti[i]
    x = x + tf[i]
  end
  return s, x
end
local before = jit.stats().ir_compiled
local ts, tx = mixed_tables(tn)
local ts2, tx2 = mixed_tables(tn)
local want_ts, want_tx = 0, 0.0
for i = 1, tn do
  want_ts = want_ts + ti[i]
  want_tx = want_tx + tf[i]
end
H.eq(ts, want_ts, "mixed table integer accumulator")
H.check(same(tx, want_tx), "mixed table double accumulator")
H.eq(ts2, want_ts, "mixed table second call")
H.check(same(tx2, want_tx), "mixed table second double result")
H.check(jit.stats().ir_compiled > before,
        "mixed table reduction did not compile in J4")

-- Pure integer and pure float table reductions must still compile.
local ints = {}
local floats = {}
for i = 1, tn do
  ints[i] = i * 7
  floats[i] = i * 1.5
end
local function int_sum (count)
  local s = 0
  for i = 1, count do s = s + ints[i] end
  return s
end
local function float_sum (count)
  local x = 0.0
  for i = 1, count do x = x + floats[i] end
  return x
end
local n0 = jit.stats().ir_compiled
H.eq(int_sum(tn), 7 * tn * (tn + 1) // 2, "pure integer table sum")
H.check(jit.stats().ir_compiled > n0, "pure integer table trace regressed")
n0 = jit.stats().ir_compiled
H.check(same(float_sum(tn), 1.5 * tn * (tn + 1) / 2),
        "pure float table sum")
H.check(jit.stats().ir_compiled > n0, "pure float table trace regressed")

-- A shape change must fall back to the interpreter without changing the
-- result.  The mixed trace was recorded with array parts; a hash copy of
-- the same values exercises the guard exit.
local hi, hf = {}, {}
for i = 1, tn do
  hi[i] = ti[i]
  hf[i] = tf[i]
end
ti, tf = hi, hf
local hs, hx = mixed_tables(tn)
H.eq(hs, want_ts, "hash-fallback integer accumulator")
H.check(same(hx, want_tx), "hash-fallback double accumulator")

jit.flush()
jit.opt.start("ir=0", "hotloop=56", "hotexit=10", "table=1", "tint=1")
jit.on()
print("jit/183_j4_mixed_reduction ok")
