-- jit/124_ir_entry_result.lua -- J4 entry-result diagnostics.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/124_ir_entry_result: skipped (no native target)")
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
H.check(st.ir_entry_checks >= 1, "trace entry was never checked")
H.check(st.ir_executed >= 1, "trace did not execute")
H.eq(st.ir_entry_failures, 0)
H.check(st.ir_last_entry_result > 0, "last J4 result was not a continue")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/124_ir_entry_result ok")
