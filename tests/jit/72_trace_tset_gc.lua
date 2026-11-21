-- jit/72_trace_tset_gc.lua -- TSET insert may allocate/GC; all frame slots
-- must stay rooted while the native helper runs.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/72_trace_tset_gc: skipped (no native target)")
  return
end

local n = 120000
local t = {}

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function fill (t, n)
  for i = 1, n do t[i] = i end
end
fill(t, n)
local st = jit.stats()
H.check(st.ir_executed >= 1, "TSET insert trace did not execute")
for _, i in ipairs({ 1, 2, 57, 1000, 65536, n - 1, n }) do
  H.eq(t[i], i)
end
H.eq(#t, n)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/72_trace_tset_gc ok")
