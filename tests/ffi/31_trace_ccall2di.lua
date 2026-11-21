-- tests/ffi/31_trace_ccall2di.lua -- (double,int)->double C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/31_trace_ccall2di: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/31_trace_ccall2di: skipped (no native target)")
  return
end

ffi.cdef[[
double ldexp(double, int);
double frexp(double, int *);
]]
local N = 1000
jit.flush()
jit.opt.start("ir=1")
jit.on()
local ldexp = ffi.C.ldexp

-- MOVE float + MOVE int.
local function sum_move (fn, x, e, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(x, e) end
  return s
end
H.near(sum_move(ldexp, 1.5, 3, N), 12000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "mixed call trace did not compile")
H.check(st.ir_executed >= 1, "mixed call trace did not execute")
H.eq(st.ir_call_exits, 0, "mixed call trace exited at the call")

-- LOADK float + LOADI int.
local compiled, executed = st.ir_compiled, st.ir_executed
local function sum_const (fn, n)
  local s = 0.0
  for _ = 1, n do s = s + fn(1.5, 3) end
  return s
end
H.near(sum_const(ldexp, N), 12000.0)
st = jit.stats()
H.check(st.ir_compiled > compiled, "constant mixed trace did not compile")
H.check(st.ir_executed > executed, "constant mixed trace did not execute")
H.eq(st.ir_call_exits, 0)

-- Same Proto with swapped/int arguments must fall back safely.
local call_exits = st.ir_call_exits
H.near(sum_move(ldexp, 3, 1, N), 6000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer first argument reached the mixed C-call op")

-- Function identity change after both argument slots were written.
call_exits = jit.stats().ir_call_exits
local frexp = ffi.C.frexp
local e = ffi.new("int[1]")
local function sum_frexp (fn, x, out, n)
  local s = 0.0
  for _ = 1, n do
    local m = fn(x, out)
    s = s + m + out[0]
  end
  return s
end
H.near(sum_frexp(frexp, 1.5, e, 1), 1.75)
H.eq(jit.stats().ir_call_exits, call_exits,
     "function change after mixed args corrupted the call state")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/31_trace_ccall2di ok")
