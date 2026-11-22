-- tests/ffi/67_trace_upvalue_int_widen.lua -- integer upvalue constants
-- widened for CCALL2FF.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/67_trace_upvalue_int_widen: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/67_trace_upvalue_int_widen: skipped (no native target)")
  return
end

ffi.cdef[[float hypotf(float, float);]]
local f = ffi.C.hypotf
local a, b = 3, 4
local function both (n)
  local s = 0.0
  for _ = 1, n do s = s + f(a, b) end
  return s
end
local function mixed (n)
  local s = 0.0
  for _ = 1, n do s = s + f(a, 4.0) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(both(1000), 5000.0)
H.near(mixed(1000), 5000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "upvalue widening did not compile")
H.check(st.ir_executed >= 2, "upvalue widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(both(1000), 5000.0)
H.near(mixed(1000), 5000.0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(both(1000), 5000.0)
H.near(mixed(1000), 5000.0)
print("ffi/67_trace_upvalue_int_widen ok")
