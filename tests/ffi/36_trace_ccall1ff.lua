-- tests/ffi/36_trace_ccall1ff.lua -- native float32-argument C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/36_trace_ccall1ff: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/36_trace_ccall1ff: skipped (no native target)")
  return
end

ffi.cdef[[
float sqrtf(float);
int abs(int);
]]
local sqrtfok, sqrtf = pcall(function () return ffi.C.sqrtf end)
if not sqrtfok then
  print("ffi/36_trace_ccall1ff: skipped (no sqrtf symbol)")
  return
end

local N = 1000
local root2 = sqrtf(2.0)
local want = root2 * N
jit.flush()
jit.opt.start("ir=1")
jit.on()

-- Literal argument compiles to OP_LOADF (float immediate), not LOADK.
local function g_const (n)
  local s = 0
  for _ = 1, n do s = s + sqrtf(2.0) end
  return s
end
H.near(g_const(N), want, 1e-6)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "float32 C-call trace did not compile")
H.check(st.ir_executed >= 1, "float32 C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "float32 C-call trace exited at the call")

-- Function parameter and float argument through MOVE.
local function g (fn, x, n)
  local s = 0
  for _ = 1, n do s = s + fn(x) end
  return s
end
local compiled = jit.stats().ir_compiled
H.near(g(sqrtf, 2.0, N), want, 1e-6)
st = jit.stats()
H.check(st.ir_compiled > compiled, "parameter float32 trace did not compile")
H.eq(st.ir_call_exits, 0)
local call_exits = st.ir_call_exits

-- Same Proto with an integer argument: the entry float guard falls back.
H.near(g(sqrtf, 2, N), want, 1e-6)
H.eq(jit.stats().ir_call_exits, call_exits,
     "integer argument reached the float32 call op")

-- Same Proto with a valid but differently-typed C function: the CALL-site
-- function guard falls back and the interpreter converts 2.0 for abs.
H.near(g(ffi.C.abs, 2.0, N), 2000.0)
H.eq(jit.stats().ir_call_exits, call_exits,
     "differently-typed function reached the float32 call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/36_trace_ccall1ff ok")
