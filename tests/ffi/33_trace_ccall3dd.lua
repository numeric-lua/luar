-- tests/ffi/33_trace_ccall3dd.lua -- three-double-argument C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/33_trace_ccall3dd: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/33_trace_ccall3dd: skipped (no native target)")
  return
end

ffi.cdef[[double fma(double, double, double);]]
local fmaok, fma = pcall(function () return ffi.C.fma end)
if not fmaok then
  print("ffi/33_trace_ccall3dd: skipped (no fma symbol)")
  return
end

local N = 1000
jit.flush()
jit.opt.start("ir=1")
jit.on()

-- All three arguments loaded by MOVE from float locals.
local function sum_xyz (fn, x, y, z, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x, y, z) end
  return s
end
H.near(sum_xyz(fma, 2.5, 4.0, 1.0, N), 11000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "three-double trace did not compile")
H.check(st.ir_executed >= 1, "three-double trace did not execute")
H.eq(st.ir_call_exits, 0, "three-double trace exited at the call")

-- Distinctive argument values: fma(1,4,2.5) = 6.5, not 11.
local call_exits = st.ir_call_exits
H.near(sum_xyz(fma, 1.0, 4.0, 2.5, N), 6500.0)
H.eq(jit.stats().ir_call_exits, call_exits)

-- Same Proto with integer arguments falls back through the float guards.
H.near(sum_xyz(fma, 2, 3, 4, N), 10000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer arguments reached the three-double C-call op")

-- A different cdata object for the same function must fail the CALL-site
-- identity guard after all three argument slots were written, then fall
-- back consistently.
local fma2 = ffi.C.fma
H.near(sum_xyz(fma2, 2.5, 4.0, 1.0, N), 11000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "function change corrupted the three-argument call state")

-- All three arguments loaded by LOADK of float constants.
local compiled = jit.stats().ir_compiled
local executed = jit.stats().ir_executed
local function sum_k (fn, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(2.5, 4.25, 1.75) end
  return s
end
H.near(sum_k(fma, N), 12375.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "constant three-double trace did not compile")
H.check(st.ir_executed > executed, "constant three-double trace did not execute")
H.eq(st.ir_call_exits, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/33_trace_ccall3dd ok")
