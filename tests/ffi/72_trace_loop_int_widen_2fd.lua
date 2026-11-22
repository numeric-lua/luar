-- tests/ffi/72_trace_loop_int_widen_2fd.lua -- integer loop variables
-- widened for (float,double)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/72_trace_loop_int_widen_2fd: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/72_trace_loop_int_widen_2fd: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double float_double_scale(float a, double b);]]
local f = helper.float_double_scale
local function sum (n)
  local s = 0.0
  for i = 1, n do s = s + f(1.5, i) end
  return s
end
local function ref (n)
  local s = 0.0
  for i = 1, n do s = s + f(1.5, i) end
  return s
end

local expected = ref(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "2FD loop widening did not compile")
H.check(st.ir_executed >= 1, "2FD loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(sum(1000), expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), expected, 0.01)
print("ffi/72_trace_loop_int_widen_2fd ok")
