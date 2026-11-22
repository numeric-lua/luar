-- tests/ffi/68_trace_loop_int_widen.lua -- runtime integer loop variables
-- widened for CCALL2FF.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/68_trace_loop_int_widen: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/68_trace_loop_int_widen: skipped (no native target)")
  return
end

ffi.cdef[[float hypotf(float, float);]]
local f = ffi.C.hypotf
local function both (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 4) end
  return s
end
local function mixed (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 4.0) end
  return s
end
local function ref (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 4) end
  return s
end

local expected = ref(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(both(1000), expected, 0.01)
H.near(mixed(1000), expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "loop widening did not compile")
H.check(st.ir_executed >= 2, "loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(both(1000), expected, 0.01)
H.near(mixed(1000), expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(both(1000), expected, 0.01)
H.near(mixed(1000), expected, 0.01)
print("ffi/68_trace_loop_int_widen ok")
