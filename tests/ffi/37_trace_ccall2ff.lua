-- tests/ffi/37_trace_ccall2ff.lua -- two-float32 native C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/37_trace_ccall2ff: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/37_trace_ccall2ff: skipped (no native target)")
  return
end

ffi.cdef[[
float hypotf(float, float);
float copysignf(float, float);
]]
local hypotfok, hypotf = pcall(function () return ffi.C.hypotf end)
if not hypotfok then
  print("ffi/37_trace_ccall2ff: skipped (no hypotf symbol)")
  return
end
local N = 1000
jit.flush()
jit.opt.start("ir=1")
jit.on()

-- Both arguments from float MOVEs.
local function sum (fn, x, y, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x, y) end
  return s
end
H.near(sum(hypotf, 3.0, 4.0, N), 5000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "two-float trace did not compile")
H.check(st.ir_executed >= 1, "two-float trace did not execute")
H.eq(st.ir_call_exits, 0, "two-float trace exited at the call")

-- LOADK/LOADF constants.
local compiled, executed = st.ir_compiled, st.ir_executed
local function sum_const (fn, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(3.0, 4.0) end
  return s
end
H.near(sum_const(hypotf, N), 5000.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "constant two-float trace did not compile")
H.check(st.ir_executed > executed, "constant two-float trace did not execute")
H.eq(st.ir_call_exits, 0)

-- Non-commutative order: copysignf(2.5, -1.0) = -2.5.
local copysignf = ffi.C.copysignf
H.near(sum(copysignf, 2.5, -1.0, N), -2500.0)

-- Same Proto with integer args falls back before the call op.
local call_exits = jit.stats().ir_call_exits
H.near(sum(hypotf, 3, 4, N), 5000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer arguments reached the two-float C-call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/37_trace_ccall2ff ok")
