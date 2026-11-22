-- tests/ffi/60_trace_faddi_2df.lua -- FADDI as the float argument of
-- (double,float)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/60_trace_faddi_2df: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/60_trace_faddi_2df: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double double_float_scale(double a, float b);]]
local f = helper.double_float_scale
local v = 2.5

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(2.5, v + 1) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 8750.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 2DF call did not compile")
H.check(st.ir_executed >= 1, "FADDI 2DF call did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 8750.0)
print("ffi/60_trace_faddi_2df ok")
