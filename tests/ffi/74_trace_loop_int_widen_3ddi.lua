-- tests/ffi/74_trace_loop_int_widen_3ddi.lua -- integer loop variables
-- widened for (double,double,int)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/74_trace_loop_int_widen_3ddi: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/74_trace_loop_int_widen_3ddi: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double ddi_scale(double a, double b, int n);]]
local f = helper.ddi_scale
local function sum (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 2.0, 2) end
  return s
end
local function ref (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 2.0, 2) end
  return s
end

local expected = ref(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "3DDI loop widening did not compile")
H.check(st.ir_executed >= 1, "3DDI loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(sum(1000), expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), expected, 0.01)
print("ffi/74_trace_loop_int_widen_3ddi ok")
