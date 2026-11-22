-- tests/ffi/61_trace_ccall3did.lua -- (double,int,double)->double calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/61_trace_ccall3did: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/61_trace_ccall3did: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double did_scale(double a, int n, double b);]]
local f = helper.did_scale

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(2.0, 3, 4.0) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 24000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CCALL3DID did not compile")
H.check(st.ir_executed >= 1, "CCALL3DID did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 24000.0)
print("ffi/61_trace_ccall3did ok")
