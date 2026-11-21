-- tests/ffi/31_trace_ccall2dd.lua -- two-double-argument C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/31_trace_ccall2dd: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/31_trace_ccall2dd: skipped (no native target)")
  return
end

ffi.cdef[[
double fmin(double, double);
double fmax(double, double);
double fmod(double, double);
]]

local N = 1000
jit.flush()
jit.opt.start("ir=1")
jit.on()
local fmin = ffi.C.fmin
local fmax = ffi.C.fmax

-- Both arguments loaded by MOVE from float locals.
local function sum_xy (fn, x, y, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x, y) end
  return s
end
H.near(sum_xy(fmin, 2.5, 1.5, N), 1500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "two-arg double trace did not compile")
H.check(st.ir_executed >= 1, "two-arg double trace did not execute")
H.eq(st.ir_call_exits, 0, "two-arg double trace exited at the call")

-- Same Proto with integer arguments (and ints read from a table) must fall
-- back through the entry float guards, not call the helper.
local t = { 2, 3 }
local call_exits = jit.stats().ir_call_exits
H.near(sum_xy(fmin, 2, 3, N), 2000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer arguments reached the two-double C-call op")
H.near(sum_xy(fmin, t[1], t[2], N), 2000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "table integer arguments reached the two-double C-call op")

-- Both arguments loaded by LOADK of float constants.
local compiled = jit.stats().ir_compiled
local executed = jit.stats().ir_executed
local function sum_k (fn, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(2.5, 1.5) end
  return s
end
H.near(sum_k(fmin, N), 1500.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "constant two-arg trace did not compile")
H.check(st.ir_executed > executed, "constant two-arg trace did not execute")
H.eq(st.ir_call_exits, 0)

-- Non-commutative function with MOVE arguments checks xmm0/xmm1 order:
-- fmod(7.5, 2.0) = 1.5; swapped order would give 2.0.
compiled = jit.stats().ir_compiled
executed = jit.stats().ir_executed
local function sum_fmod (fn, x, y, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x, y) end
  return s
end
H.near(sum_fmod(ffi.C.fmod, 7.5, 2.0, N), 1500.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "argument-order trace did not compile")
H.check(st.ir_executed > executed, "argument-order trace did not execute")
H.eq(st.ir_call_exits, 0)

-- Function identity changes after both argument slots were written: the
-- CALL-site guard must fall back with consistent slots.
call_exits = jit.stats().ir_call_exits
H.near(sum_xy(fmax, 2.5, 1.5, N), 2500.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "function change corrupted the two-argument call state")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/31_trace_ccall2dd ok")
