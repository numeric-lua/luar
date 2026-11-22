-- tests/ffi/51_trace_faddi_3arg.lua -- FADDI results as arguments to a
-- three-argument native call.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/51_trace_faddi_3arg: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/51_trace_faddi_3arg: skipped (no native target)")
  return
end

ffi.cdef[[double fma(double, double, double);]]
local fma = ffi.C.fma
local v = 2.5

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + fma(v + 1, v + 2, 3.0) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 18750.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 3-arg call did not compile")
H.check(st.ir_executed >= 1, "FADDI 3-arg call did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 18750.0)
print("ffi/51_trace_faddi_3arg ok")
