-- tests/ffi/71_trace_loop_int_widen_2id.lua -- integer loop variables
-- widened for (int,double)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/71_trace_loop_int_widen_2id: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/71_trace_loop_int_widen_2id: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double int_double_sum(int a, double b);]]
local f = helper.int_double_sum
local function sum (n)
  local s = 0.0
  for i = 1, n do s = s + f(2, i) end
  return s
end
local function ref (n)
  local s = 0.0
  for i = 1, n do s = s + f(2, i) end
  return s
end

local expected = ref(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "2ID loop widening did not compile")
H.check(st.ir_executed >= 1, "2ID loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(sum(1000), expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), expected, 0.01)
print("ffi/71_trace_loop_int_widen_2id ok")
