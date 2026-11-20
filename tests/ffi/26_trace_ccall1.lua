-- tests/ffi/26_trace_ccall1.lua -- native one-integer-argument C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/26_trace_ccall1: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/26_trace_ccall1: skipped (no native target)")
  return
end

ffi.cdef[[int abs(int);]]

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sum_abs (n)
  local abs = ffi.C.abs
  local s = 0
  for i = 1, n do s = s + abs(i) end
  return s
end
H.eq(sum_abs(1000), 500500)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "native C-call trace did not compile")
H.check(st.ir_executed >= 1, "native C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "native C-call trace exited at the call")

-- A negative argument takes the same helper path and must stay signed.
local function sum_abs_neg (n)
  local abs = ffi.C.abs
  local s = 0
  local x = -7
  for _ = 1, n do s = s + abs(x) end
  return s
end
H.eq(sum_abs_neg(1000), 7000)
st = jit.stats()
H.check(st.ir_executed >= 1, "negative argument trace did not execute")
H.eq(st.ir_call_exits, 0)

-- A constant argument is loaded by LOADI right before the call.  The
-- integer guard must run after that LOADI, not at trace entry (a previous
-- __call trampoline leaves the function cdata in the argument slot).
local function sum_abs_const (n)
  local abs = ffi.C.abs
  local s = 0
  for _ = 1, n do s = s + abs(-42) end
  return s
end
H.eq(sum_abs_const(1000), 42000)
st = jit.stats()
H.check(st.ir_executed >= 1, "constant argument trace did not execute")
H.eq(st.ir_call_exits, 0)

-- The same traced Proto must fall back safely when its argument changes
-- from integer to float; the float payload must never reach CCALL1I.
local abs_fn = ffi.C.abs
local function sum_arg (fn, x, n)
  local s = 0
  for _ = 1, n do s = s + fn(x) end
  return s
end
local compiled = jit.stats().ir_compiled
H.eq(sum_arg(abs_fn, 5, 1000), 5000)
st = jit.stats()
H.check(st.ir_compiled > compiled, "integer argument trace did not compile")
local call_exits = st.ir_call_exits
H.eq(sum_arg(abs_fn, 5.5, 1000), 5000)
H.eq(jit.stats().ir_call_exits, call_exits,
     "float argument reached the native C-call op")

-- A float argument cannot use the integer helper; the loop must fall back
-- and still produce the interpreter result.
local function sum_abs_float (n)
  local abs = ffi.C.abs
  local s = 0
  for i = 1, n do s = s + abs(i / 2) end
  return s
end
H.eq(sum_abs_float(1000), 250000)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/26_trace_ccall1 ok")
