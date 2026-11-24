-- jit/189_tint_kind_collision.lua -- affine fills and keyed sums must use
-- their own intrinsic slots.
--
-- The affine-fill intrinsic reused kind 4, which is the keyed-sum kind.
-- A hot keyed sum then dispatched to the raw-table fill helper: it stopped
-- the loop after the interpreted part and left the accumulator unchanged
-- (and could overwrite array entries).  Run both shapes in one Proto so a
-- future kind collision is caught even when each helper works alone.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/189_tint_kind_collision: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 500

-- A keyed sum after an affine fill in the same function: two intrinsics
-- with different kinds, both live in this Proto.
local function fill_then_sum (n)
  local t = {}
  for i = 1, n do t[i] = i + 3 end
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s, t
end

-- The same shape with a separate key array exercises the keyed-sum helper
-- (kind 4), whose previous default-case dispatch was the only thing keeping
-- it alive before kind 4 was taken.
local function fill_then_keyed (n)
  local t, k = {}, {}
  for i = 1, n do t[i] = i * 2 end
  for i = 1, n do k[i] = "k" .. i end
  for i = 1, n do t[k[i]] = i end
  local s = 0
  for i = 1, n do s = s + t[k[i]] end
  return s, t
end

local function compare (name, f, n)
  jit.flush()
  jit.off()
  local want, wt = f(n)
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got, gt = f(n)
  local used = jit.stats().tint_hits - before
  H.eq(got, want, name .. ": result")
  H.check(used >= 2, name .. ": expected two intrinsic kinds, got " .. used)
  for i = 1, n do
    H.eq(gt[i], wt[i], name .. ": element " .. i)
    H.eq(math.type(gt[i]), math.type(wt[i]), name .. ": element type " .. i)
  end
  H.eq(gt.marker, wt.marker, name .. ": hash key")
end

compare("fill+array sum", fill_then_sum, N)
compare("affine fill+keyed sum", function (n)
  local t, k = {}, {}
  for i = 1, n do t[i] = i + 3 end
  for i = 1, n do k[i] = "k" .. i end
  for i = 1, n do t[k[i]] = i end
  local s = 0
  for i = 1, n do s = s + t[k[i]] end
  return s, t
end, N)

-- A keyed sum whose table already contains array entries must not be
-- rewritten by the old affine-fill dispatch.
local function keyed_after_array (n)
  local t, k = {10, 20, 30, 40}, {}
  for i = 1, n do k[i] = i end
  local s = 0
  for i = 1, n do s = s + t[k[i]] end
  return s, t
end

local function check_keyed (n)
  local s, t = keyed_after_array(n)
  H.eq(s, 10 + 20 + 30 + 40, "keyed sum over array part")
  H.eq(t[1], 10, "array entry 1 preserved")
  H.eq(t[4], 40, "array entry 4 preserved")
end

check_keyed(4)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/189_tint_kind_collision ok")
