-- tests/ffi/66_trace_mixed_widen_2ff.lua -- one integer literal and one
-- float argument widened for CCALL2FF.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/66_trace_mixed_widen_2ff: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/66_trace_mixed_widen_2ff: skipped (no native target)")
  return
end

ffi.cdef[[float hypotf(float, float);]]
local f = ffi.C.hypotf
local function lhs_int (n)
  local s = 0.0
  for _ = 1, n do s = s + f(3, 4.0) end
  return s
end
local function rhs_int (n)
  local s = 0.0
  for _ = 1, n do s = s + f(3.0, 4) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(lhs_int(1000), 5000.0)
H.near(rhs_int(1000), 5000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "mixed widening did not compile")
H.check(st.ir_executed >= 2, "mixed widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(lhs_int(1000), 5000.0)
H.near(rhs_int(1000), 5000.0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(lhs_int(1000), 5000.0)
H.near(rhs_int(1000), 5000.0)
print("ffi/66_trace_mixed_widen_2ff ok")
