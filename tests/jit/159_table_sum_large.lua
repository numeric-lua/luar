-- jit/159_table_sum_large.lua -- raw table sums must cover loops larger
-- than the old 4M-iteration safety cap.
--
-- The array and keyed sum intrinsics refused a remaining count above
-- 1 << 22.  The refusal happened at every backedge, so a 5M-element sum
-- paid a C helper call per iteration and ran slower than the interpreter.
-- A sum performs no allocation, and its bounds are already checked against
-- the table's 32-bit array size, so the cap is unnecessary.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/159_table_sum_large: skipped (no native target)")
  return
end

local N = 4200000
local t, keys = {}, {}
for i = 1, N do
  t[i] = i
  keys[i] = i
end

jit.flush()
jit.off()
jit.opt.start("hotloop=1", "table=0", "tint=1")

local function sum (n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end

local function keyed (tab, k, n)
  local s = 0
  for i = 1, n do s = s + tab[k[i]] end
  return s
end

jit.flush()
jit.on()

local want = N * (N + 1) // 2
local before = jit.stats().tint_hits
H.eq(sum(N), want, "large raw array sum")
H.check(jit.stats().tint_hits > before,
        "large array sum did not use the array intrinsic")

before = jit.stats().tint_hits
H.eq(keyed(t, keys, N), want, "large keyed array sum")
H.check(jit.stats().tint_hits > before,
        "large keyed sum did not use the keyed intrinsic")

-- The intrinsic is still bounded by the live array size: a loop that runs
-- past the array must fall back and raise like the interpreter.
keys[N] = nil
H.raises(function () return keyed(t, keys, N) end, "arithmetic")

jit.opt.start("hotloop=56", "table=0", "tint=1")
jit.flush()
print("jit/159_table_sum_large ok")
