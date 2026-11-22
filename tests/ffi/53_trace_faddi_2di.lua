-- tests/ffi/53_trace_faddi_2di.lua -- FADDI as the double argument of
-- double(int,double) native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/53_trace_faddi_2di: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/53_trace_faddi_2di: skipped (no native target)")
  return
end

ffi.cdef[[double ldexp(double, int);]]
local ldexp = ffi.C.ldexp
local v = 2.5

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + ldexp(v + 1, 2) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 14000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 2DI call did not compile")
H.check(st.ir_executed >= 1, "FADDI 2DI call did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 14000.0)
print("ffi/53_trace_faddi_2di ok")
