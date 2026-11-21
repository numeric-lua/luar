-- tests/ffi/40_trace_float_addi_call.lua -- FADDI arguments to native calls
-- must not be treated as integer ADDI (which passes float bits as int).
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/40_trace_float_addi_call: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/40_trace_float_addi_call: skipped (no native target)")
  return
end

ffi.cdef[[double fabs(double);]]
local fabs = ffi.C.fabs
local v = 2.5

local function f (n)
  local s = 0.0
  for _ = 1, n do s = s + fabs(v + 1) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(f(1000), 3500.0)
H.check(jit.stats().ir_compiled >= 1, "FADDI call trace did not compile")

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(f(1000), 3500.0)
print("ffi/40_trace_float_addi_call ok")
