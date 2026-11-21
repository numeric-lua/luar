-- tests/ffi/35_trace_upvalue_ccall.lua -- top-level cdata upvalue calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/35_trace_upvalue_ccall: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/35_trace_upvalue_ccall: skipped (no native target)")
  return
end

ffi.cdef[[int abs(int);]]
local abs = ffi.C.abs

jit.flush()
jit.opt.start("ir=1")
jit.on()

-- The canonical top-level upvalue shape: GETUPVAL loads the cdata function.
local function g (n)
  local s = 0
  for i = 1, n do s = s + abs(i) end
  return s
end
H.eq(g(1000), 500500)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "upvalue C-call trace did not compile")
H.check(st.ir_executed >= 1, "upvalue C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "upvalue C-call trace exited at the call")

-- Negative arguments through the same upvalue.
local function g_neg (n)
  local s = 0
  local x = -7
  for _ = 1, n do s = s + abs(x) end
  return s
end
local compiled = jit.stats().ir_compiled
H.eq(g_neg(1000), 7000)
st = jit.stats()
H.check(st.ir_compiled > compiled, "negative upvalue trace did not compile")
H.eq(st.ir_call_exits, 0)

-- Two closures of the same Proto with different upvalues: the ULOAD
-- closure-identity check must reject the second closure and fall back.
local function make (fn)
  return function (n)
    local s = 0
    for i = 1, n do s = s + fn(i) end
    return s
  end
end
local g1 = make(abs)
local g2 = make(function (x) return x * 2 end)
compiled = jit.stats().ir_compiled
H.eq(g1(1000), 500500)
st = jit.stats()
H.check(st.ir_compiled > compiled, "closure upvalue trace did not compile")
local call_exits = st.ir_call_exits
H.eq(g2(1000), 1001000)
H.eq(jit.stats().ir_call_exits, call_exits,
     "different closure reached the native call op")

-- A mutable upvalue: after the cdata function is replaced by a Lua
-- function, ULOAD loads the new value and the call must fall back safely.
local fn = abs
local function g_mut (n)
  local s = 0
  for i = 1, n do s = s + fn(i) end
  return s
end
compiled = jit.stats().ir_compiled
H.eq(g_mut(1000), 500500)
st = jit.stats()
H.check(st.ir_compiled > compiled, "mutable upvalue trace did not compile")
call_exits = st.ir_call_exits
fn = function (x) return x * 2 end
H.eq(g_mut(1000), 1001000)
H.eq(jit.stats().ir_call_exits, call_exits,
     "Lua-function upvalue reached the cdata call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/35_trace_upvalue_ccall ok")
