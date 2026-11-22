-- jit/123_ir_entry_stats.lua -- J4 entry diagnostics counters.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/123_ir_entry_stats: skipped (no native target)")
  return
end

local function f (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f(1000), 500500)
local st = jit.stats()
H.check(type(st.ir_entry_checks) == "number")
H.check(type(st.ir_entry_pc_mismatch) == "number")
H.check(type(st.ir_entry_base_mismatch) == "number")
H.check(st.ir_entry_checks >= 1, "trace entry was never checked")
H.check(st.ir_executed >= 1, "trace did not execute")
H.eq(st.ir_entry_pc_mismatch, 0)
H.eq(st.ir_entry_base_mismatch, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/123_ir_entry_stats ok")
