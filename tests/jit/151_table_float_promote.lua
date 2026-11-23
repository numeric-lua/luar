-- jit/151_table_float_promote.lua -- integer-accumulator raw table sums
-- promote to float on the first float element.
--
-- The interpreter's mixed addition turns an integer accumulator into a
-- float as soon as a float element is added.  The raw table/keyed sum
-- intrinsics only accepted all-integer elements for an integer accumulator
-- and bailed to the interpreter, so `local s = 0; s = s + t[i]` over a
-- float array stayed interpreted.  tint_hits makes the intrinsic run
-- observable instead of relying on wall-clock timing.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/151_table_float_promote: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=1", "tint=1")
jit.on()

local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end

local function sumk (t, keys, n)
  local s = 0
  for i = 1, n do s = s + t[keys[i]] end
  return s
end

local f = {}
for i = 1, 500 do f[i] = i + 0.5 end
local want = 0.0
for i = 1, 500 do want = want + f[i] end

H.check(math.type(want) == "float", "test setup produced an integer")

jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=1", "tint=1")
jit.on()
H.eq(sum(f, 500), want, "float array sum")
H.check(math.type(sum(f, 500)) == "float", "sum did not promote")
local hits = jit.stats().tint_hits
H.check(hits > 0, "float array sum did not run the intrinsic")

local keys = {}
for i = 1, 500 do keys[i] = "k" .. i end
local g = {}
for i = 1, 500 do g[keys[i]] = i + 0.5 end
jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=1", "tint=1")
jit.on()
H.eq(sumk(g, keys, 500), want, "keyed float sum")
H.check(math.type(sumk(g, keys, 500)) == "float",
        "keyed sum did not promote")
H.check(jit.stats().tint_hits > 0, "keyed float sum did not run the intrinsic")

-- Mixed integer/float elements promote at the first float and keep adding
-- both kinds, exactly like the interpreter.
local mixed = {}
for i = 1, 200 do
  if i % 3 == 0 then mixed[i] = i + 0.25 else mixed[i] = i end
end
local mwant = 0.0
for i = 1, 200 do mwant = mwant + mixed[i] end
jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=1", "tint=1")
jit.on()
H.eq(sum(mixed, 200), mwant, "mixed array sum")
H.eq(math.type(sum(mixed, 200)), "float", "mixed array did not promote")

-- A non-numeric element must still fall back to the interpreter and raise
-- the normal error instead of being summed as a zero.
local bad = {1, 2, 3, "x"}
jit.flush()
jit.opt.start("hotloop=1", "ir=0", "table=1", "tint=1")
jit.on()
H.raises(function () return sum(bad, 4) end, "attempt to add")

jit.flush()
print("jit/151_table_float_promote ok")
