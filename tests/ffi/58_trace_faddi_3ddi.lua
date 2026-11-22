-- tests/ffi/58_trace_faddi_3ddi.lua -- FADDI results as the two double
-- arguments of (double,double,int)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/58_trace_faddi_3ddi: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/58_trace_faddi_3ddi: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double ddi_scale(double a, double b, int n);]]
local f = helper.ddi_scale
local v = 2.5

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(v + 1, v + 2, 4) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 63000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 3DDI call did not compile")
H.check(st.ir_executed >= 1, "FADDI 3DDI call did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 63000.0)
print("ffi/58_trace_faddi_3ddi ok")
