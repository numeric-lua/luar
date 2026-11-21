-- tests/ffi/44_trace_ccall2fi.lua -- (float,int)->float native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/44_trace_ccall2fi: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/44_trace_ccall2fi: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[float scale_fi(float x, int n);]]
local f = helper.scale_fi

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(2.5, 3) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 7500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CCALL2FI did not compile")
H.check(st.ir_executed >= 1, "CCALL2FI did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 7500.0)
print("ffi/44_trace_ccall2fi ok")
