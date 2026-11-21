-- tests/ffi/41_trace_faddi_constant_call.lua -- constant float add-immediate
-- arguments execute natively; variable sources still fall back safely.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/41_trace_faddi_constant_call: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/41_trace_faddi_constant_call: skipped (no native target)")
  return
end

ffi.cdef[[double fabs(double);]]
local fabs = ffi.C.fabs

local function k (n)
  local s = 0.0
  for _ = 1, n do s = s + fabs(2.5 + 1) end
  return s
end
local v = 2.5
local function u (n)
  local s = 0.0
  for _ = 1, n do s = s + fabs(v + 1) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(k(1000), 3500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "constant FADDI call did not compile")
H.check(st.ir_executed >= 1, "constant FADDI call did not execute")
H.near(u(1000), 3500.0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(k(1000), 3500.0)
H.near(u(1000), 3500.0)
print("ffi/41_trace_faddi_constant_call ok")
