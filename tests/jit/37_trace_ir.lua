-- jit/37_trace_ir.lua -- tracing-IR construction and exit-state selftest.
local H = dofile("tests/helpers.lua")

local ir = jit.trace_ir()
H.eq(type(ir), "table")
H.eq(ir.version, 1)
H.eq(ir.max_ops, 64)
H.eq(ir.max_exits, 4)
H.eq(ir.selftest(), true)
H.eq(ir.guard_selftest(), true)
if jit.status() then
  H.eq(ir.exec_selftest(), true)
  H.eq(ir.side_exec_selftest(), true)
  H.eq(ir.loop_exec_selftest(), true)
  H.eq(ir.add_selftest(), true)
end

print("jit/37_trace_ir ok")
