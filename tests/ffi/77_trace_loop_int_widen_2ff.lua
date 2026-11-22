-- tests/ffi/77_trace_loop_int_widen_2ff.lua -- both arguments from integer
-- loop expressions widened for CCALL2FF.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/77_trace_loop_int_widen_2ff: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/77_trace_loop_int_widen_2ff: skipped (no native target)")
  return
end

ffi.cdef[[float hypotf(float, float);]]
local f = ffi.C.hypotf
local function same (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, i) end
  return s
end
local function next_one (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, i + 1) end
  return s
end
local function ref_same (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, i) end
  return s
end
local function ref_next (n)
  local s = 0.0
  for i = 1, n do s = s + f(i, i + 1) end
  return s
end

local same_expected = ref_same(1000)
local next_expected = ref_next(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(same(1000), same_expected, 0.01)
H.near(next_one(1000), next_expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "2FF loop widening did not compile")
H.check(st.ir_executed >= 2, "2FF loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(same(1000), same_expected, 0.01)
H.near(next_one(1000), next_expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(same(1000), same_expected, 0.01)
H.near(next_one(1000), next_expected, 0.01)
print("ffi/77_trace_loop_int_widen_2ff ok")
