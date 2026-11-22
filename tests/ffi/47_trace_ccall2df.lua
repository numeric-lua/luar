-- tests/ffi/47_trace_ccall2df.lua -- (double,float)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/47_trace_ccall2df: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/47_trace_ccall2df: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double double_float_scale(double a, float b);]]
local f = helper.double_float_scale

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(1.5, 2.5) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 3750.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CCALL2DF did not compile")
H.check(st.ir_executed >= 1, "CCALL2DF did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 3750.0)
print("ffi/47_trace_ccall2df ok")
