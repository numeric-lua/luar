-- jit/07_table_write.lua -- experimental opt-in raw integer table stores.
local H = dofile("tests/helpers.lua")

if jit.status() == false then
  print("jit/07_table_write: skipped (no native target)")
  return
end

jit.flush()
jit.on()
jit.opt.start("table=1")

local function fill (t, n)
  for i = 1, n do t[i] = i end
end
local t = {}
fill(t, 20000)
H.eq(t[1], 1)
H.eq(t[20000], 20000)
H.eq(#t, 20000)
H.check(jit.stats().compiled >= 1, "table write did not compile")

-- growth beyond the current array part falls back per iteration
local g = {}
fill(g, 5000)
H.eq(g[5000], 5000)

-- float stores are not in the integer fast path but must stay correct
local function fillf (t, n)
  for i = 1, n do t[i] = i * 0.5 end
end
local f = {}
fillf(f, 5000)
H.eq(f[100], 50.0)

jit.opt.start("table=0")
print("jit/07_table_write ok")
