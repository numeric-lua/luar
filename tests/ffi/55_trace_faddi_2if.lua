-- tests/ffi/55_trace_faddi_2if.lua -- FADDI as the float argument of
-- (int,float)->float native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/55_trace_faddi_2if: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/55_trace_faddi_2if: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[float int_float_scale(int n, float x);]]
local f = helper.int_float_scale
local v = 2.5

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(2, v + 1) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 7000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 2IF call did not compile")
H.check(st.ir_executed >= 1, "FADDI 2IF call did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 7000.0)
print("ffi/55_trace_faddi_2if ok")
