-- jit/202_table_nil_store.lua -- native `t[i] = nil` array-range clears.
--
-- The nil-store intrinsic clears array slots in one C pass when the whole
-- run is inside the existing array part and the table has no metatable.
-- Partial, descending and strided ranges are supported; hash keys and
-- entries outside the run are untouched.  Metatables and runs past the
-- array part fall back to the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/202_table_nil_store: skipped (no native target)")
  return
end

local function count (t)
  local c = 0
  for _ in pairs(t) do c = c + 1 end
  return c
end

local function filled (n)
  local t = { marker = "keep" }
  for i = 1, n do t[i] = i end
  return t
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local function clear_all (t, n)
  for i = 1, n do t[i] = nil end
end

local function clear_part (t, lo, hi)
  for i = lo, hi do t[i] = nil end
end

local function clear_down (t, n)
  for i = n, 1, -1 do t[i] = nil end
end

local function clear_step (t, n)
  for i = 1, n, 3 do t[i] = nil end
end

local t = filled(200)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
clear_all(t, 200)
H.check(jit.stats().tint_hits > before, "full clear did not use the intrinsic")
H.eq(count(t), 1, "only the marker remains")
H.eq(t.marker, "keep", "hash key preserved")
H.eq(t[1], nil, "first cleared")
H.eq(t[200], nil, "last cleared")

t = filled(200)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_part(t, 50, 150)
H.check(jit.stats().tint_hits > before, "partial clear did not use the intrinsic")
H.eq(t[49], 49, "entry before range")
H.eq(t[50], nil, "first cleared in range")
H.eq(t[150], nil, "last cleared in range")
H.eq(t[151], 151, "entry after range")

t = filled(200)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_down(t, 200)
H.check(jit.stats().tint_hits > before, "descending clear did not use the intrinsic")
H.eq(t[1], nil, "descending first")
H.eq(t[200], nil, "descending last")

t = filled(200)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_step(t, 200)
H.check(jit.stats().tint_hits > before, "strided clear did not use the intrinsic")
H.eq(t[1], nil, "strided 1")
H.eq(t[2], 2, "strided 2 kept")
H.eq(t[200], 200, "strided 200 kept")

-- A run past the array part stays interpreted but is still correct.
t = filled(100)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_all(t, 200)
H.eq(jit.stats().tint_hits, before, "out-of-array clear must not use the intrinsic")
H.eq(count(t), 1, "out-of-array clear result")
H.eq(t[100], nil, "out-of-array last array key")

-- An absent key with __newindex invokes the handler; the intrinsic declines.
local hits = 0
local meta = setmetatable({}, {
  __newindex = function () hits = hits + 1 end,
})
for i = 1, 200 do rawset(meta, i, i) end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_all(meta, 200)
H.eq(jit.stats().tint_hits, before, "metatable clear must not use the intrinsic")
H.eq(hits, 0, "existing fields bypass __newindex")
H.eq(meta[1], nil, "metatable array clear")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/202_table_nil_store ok")
