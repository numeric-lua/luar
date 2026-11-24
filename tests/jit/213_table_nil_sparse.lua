-- jit/213_table_nil_sparse.lua -- nil-range clears over sparse tables.
--
-- The intrinsic now clears array slots directly and removes hash entries
-- through luaH_setint for keys outside the array part.  Hash keys outside
-- the range survive.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/213_table_nil_sparse: skipped (no native target)")
  return
end

local function count (t)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  return n
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local t = { marker = "keep", [1500] = "outside" }
for i = 1, 1000, 7 do t[i] = i end
local function clear (tab, n)
  for i = 1, n do tab[i] = nil end
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
clear(t, 1000)
H.check(jit.stats().tint_hits > before, "sparse clear did not use the intrinsic")
H.eq(count(t), 2, "only outside keys remain")
H.eq(t.marker, "keep", "hash marker preserved")
H.eq(t[1500], "outside", "outside key preserved")
for i = 1, 1000, 7 do H.eq(t[i], nil, "cleared key " .. i) end

-- A range that starts above the array part still removes hash entries.
local u = { [2000] = 1, [2001] = 2, [3000] = 3 }
local function clear_high (tab, lo, hi)
  for i = lo, hi do tab[i] = nil end
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_high(u, 2000, 2500)
H.check(jit.stats().tint_hits > before, "high sparse clear did not use the intrinsic")
H.eq(u[2000], nil, "high key 1")
H.eq(u[2001], nil, "high key 2")
H.eq(u[3000], 3, "outside high key")

-- A metatable still falls back to the interpreter.
local seen = 0
local m = setmetatable({}, { __newindex = function () seen = seen + 1 end })
for i = 1, 1000 do rawset(m, i, i) end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear(m, 1000)
H.eq(jit.stats().tint_hits, before, "metatable clear must not use the intrinsic")
H.eq(seen, 0, "existing fields bypass __newindex")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/213_table_nil_sparse ok")
