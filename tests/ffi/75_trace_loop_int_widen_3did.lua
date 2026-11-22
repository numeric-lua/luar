-- tests/ffi/75_trace_loop_int_widen_3did.lua -- integer loop variables
-- widened for (double,int,double)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/75_trace_loop_int_widen_3did: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/75_trace_loop_int_widen_3did: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double did_scale(double a, int n, double b);]]
local f = helper.did_scale
local function sum (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 2, 2.0) end
  return s
end
local function ref (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, 2, 2.0) end
  return s
end

local expected = ref(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "3DID loop widening did not compile")
H.check(st.ir_executed >= 1, "3DID loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(sum(1000), expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), expected, 0.01)
print("ffi/75_trace_loop_int_widen_3did ok")
