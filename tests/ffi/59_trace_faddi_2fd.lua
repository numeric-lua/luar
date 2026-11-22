-- tests/ffi/59_trace_faddi_2fd.lua -- FADDI as the float argument of
-- (float,double)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/59_trace_faddi_2fd: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/59_trace_faddi_2fd: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double float_double_scale(float a, double b);]]
local f = helper.float_double_scale
local v = 2.5

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(v + 1, 2.5) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 8750.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 2FD call did not compile")
H.check(st.ir_executed >= 1, "FADDI 2FD call did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 8750.0)
print("ffi/59_trace_faddi_2fd ok")
