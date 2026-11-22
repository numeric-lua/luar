-- tests/ffi/69_trace_loop_int_widen_1dd.lua -- integer loop variables
-- widened for double(double) native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/69_trace_loop_int_widen_1dd: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/69_trace_loop_int_widen_1dd: skipped (no native target)")
  return
end

ffi.cdef[[double fabs(double); double sqrt(double);]]
local fabs, sqrt = ffi.C.fabs, ffi.C.sqrt
local function abs_sum (n)
  local s = 0.0
  for i = 1, n do s = s + fabs(i) end
  return s
end
local function sqrt_sum (n)
  local s = 0.0
  for i = 1, n do s = s + sqrt(i) end
  return s
end
local function abs_ref (n)
  local s = 0.0
  for i = 1, n do s = s + fabs(i) end
  return s
end
local function sqrt_ref (n)
  local s = 0.0
  for i = 1, n do s = s + sqrt(i) end
  return s
end

local abs_expected = abs_ref(1000)
local sqrt_expected = sqrt_ref(1000)

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(abs_sum(1000), abs_expected, 0.01)
H.near(sqrt_sum(1000), sqrt_expected, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "1DD loop widening did not compile")
H.check(st.ir_executed >= 2, "1DD loop widening did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.near(abs_sum(1000), abs_expected, 0.01)
H.near(sqrt_sum(1000), sqrt_expected, 0.01)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(abs_sum(1000), abs_expected, 0.01)
H.near(sqrt_sum(1000), sqrt_expected, 0.01)
print("ffi/69_trace_loop_int_widen_1dd ok")
