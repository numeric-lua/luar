-- jit/137_fast_table_sum.lua -- the fast `s = s + t[i]` emitter must
-- advance the loop index and materialise the GETTABLE result slot.
--
-- With the raw intrinsics disabled (`tint=0`), this path owns the whole
-- loop.  It used to add the stale `fw_step` field (zero for this shape) to
-- the index and never wrote the element register, so a local assigned from
-- the table element stayed at its pre-compilation value.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/137_fast_table_sum: skipped (no native target)")
  return
end

jit.off()
jit.flush()

local function fill (t, n, f)
  for i = 1, n do t[i] = f(i) end
end

local t = {}
fill(t, 1000, function (i) return i end)

jit.opt.start("table=1", "tint=0", "hotloop=2")
jit.on()

local function sum (t, n)
  local s = 0
  local x
  for i = 1, n do
    x = t[i]
    s = s + x
  end
  return s, x
end

local s, x = sum(t, 200)
H.eq(s, 200 * 201 / 2, "fast table sum")
H.eq(x, 200, "fast table sum did not materialise the element slot")
local e0 = jit.stats().exits
local s2, x2 = sum(t, 200)
H.eq(s2, 200 * 201 / 2, "second fast table sum")
H.eq(x2, 200, "second fast table sum element slot")
H.eq(jit.stats().exits, e0,
     "the fast table sum exited instead of running natively")
H.check(jit.stats().compiled >= 1, "fast table sum did not compile")

-- The range ends exactly at the table's last element: count + index is the
-- last key, not one past it.
local exact = {}
fill(exact, 200, function (i) return i end)
local s3, x3 = sum(exact, 200)
H.eq(s3, 200 * 201 / 2, "table-sized fast sum")
H.eq(x3, 200, "table-sized fast sum element slot")

-- Float accumulators use the same element read.
local function sumf (t, n)
  local s = 0.0
  local x
  for i = 1, n do
    x = t[i]
    s = s + x
  end
  return s, x
end
local sf, xf = sumf(t, 200)
H.eq(sf, 200 * 201 / 2 + 0.0, "float accumulator fast sum")
H.eq(xf, 200, "float accumulator element slot")

jit.opt.start("table=0", "tint=1", "hotloop=56")
jit.flush()
print("jit/137_fast_table_sum ok")
