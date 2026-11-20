-- tests/ffi/27_trace_ccall1dd.lua -- native double arg/return C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/27_trace_ccall1dd: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/27_trace_ccall1dd: skipped (no native target)")
  return
end

ffi.cdef[[double fabs(double);]]

jit.flush()
jit.opt.start("ir=1")
jit.on()

-- Argument loaded by MOVE from a float local.
local function sum_fabs_move (n)
  local fabs = ffi.C.fabs
  local x = 2.5
  local s = 0.0
  for _ = 1, n do s = s + fabs(x) end
  return s
end
H.near(sum_fabs_move(1000), 2500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "double C-call trace did not compile")
H.check(st.ir_executed >= 1, "double C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "double C-call trace exited at the call")

-- Argument loaded by LOADK of a float constant.
local compiled = st.ir_compiled
local function sum_fabs_k (n)
  local fabs = ffi.C.fabs
  local s = 0.0
  for _ = 1, n do s = s + fabs(2.5) end
  return s
end
H.near(sum_fabs_k(1000), 2500.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "constant double argument trace did not compile")
H.check(st.ir_executed >= 1, "constant double argument trace did not execute")
H.eq(st.ir_call_exits, 0)

-- Same Proto: double trace first, then an integer argument must fall back.
local fabs_fn = ffi.C.fabs
local function sum_arg (fn, x, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x) end
  return s
end
compiled = jit.stats().ir_compiled
H.near(sum_arg(fabs_fn, 2.5, 1000), 2500.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "double argument trace did not compile")
local call_exits = st.ir_call_exits
H.near(sum_arg(fabs_fn, 2, 1000), 2000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer argument reached the double C-call op")

-- Same Proto in the other order: an integer trace must fall back on a
-- double argument instead of reusing the CCALL1DI path.
local function sum_arg_i (fn, x, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x) end
  return s
end
compiled = jit.stats().ir_compiled
H.near(sum_arg_i(fabs_fn, 2, 1000), 2000.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "integer argument trace did not compile")
call_exits = st.ir_call_exits
H.near(sum_arg_i(fabs_fn, 2.5, 1000), 2500.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "double argument reached the integer C-call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/27_trace_ccall1dd ok")
