-- tests/ffi/50_trace_faddi_2arg_rhs.lua -- FADDI results as the second
-- argument of a two-argument native call.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/50_trace_faddi_2arg_rhs: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/50_trace_faddi_2arg_rhs: skipped (no native target)")
  return
end

ffi.cdef[[double fmin(double, double); double fmax(double, double);]]
local fmin, fmax = ffi.C.fmin, ffi.C.fmax
local v = 2.5

local function lo (n)
  local s = 0.0
  for _ = 1, n do s = s + fmin(3.0, v + 1) end
  return s
end
local function hi (n)
  local s = 0.0
  for _ = 1, n do s = s + fmax(3.0, v + 1) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(lo(1000), 3000.0)
H.near(hi(1000), 3500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "FADDI RHS calls did not compile")
H.check(st.ir_executed >= 2, "FADDI RHS calls did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(lo(1000), 3000.0)
H.near(hi(1000), 3500.0)
print("ffi/50_trace_faddi_2arg_rhs ok")
