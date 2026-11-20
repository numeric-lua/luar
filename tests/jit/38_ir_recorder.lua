-- jit/38_ir_recorder.lua -- interpreter-side J4 IR recording.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/38_ir_recorder: skipped (no native target)")
  return
end

jit.flush()
jit.on()
local s = 0
for i = 1, 1000 do s = s + i end
H.eq(s, 500500)
local st = jit.stats()
H.check(st.ir_traces >= 1, "no J4 IR trace recorded")
H.check(st.ir_ops >= 2, "J4 IR trace has too few operations")
H.check(st.ir_aborts >= 0, "missing IR abort counter")
H.check(st.ir_install_rejects >= 1, "rejected trace not counted")
H.eq(st.ir_compiled, 0)

jit.flush()
H.eq(jit.stats().ir_traces, 0)
H.eq(jit.stats().ir_ops, 0)
H.eq(jit.stats().ir_install_rejects, 0)
jit.on()
print("jit/38_ir_recorder ok")
