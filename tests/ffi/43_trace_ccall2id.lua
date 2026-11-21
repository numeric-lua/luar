-- tests/ffi/43_trace_ccall2id.lua -- (int,double)->double native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/43_trace_ccall2id: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/43_trace_ccall2id: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double int_double_sum(int a, double b);]]
local f = helper.int_double_sum

local function sum_const (n)
  local s = 0.0
  for _ = 1, n do s = s + f(2, 2.5) end
  return s
end
local function sum_move (a, b, n)
  local s = 0.0
  for _ = 1, n do s = s + f(a, b) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum_const(1000), 4500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CCALL2ID did not compile")
H.check(st.ir_executed >= 1, "CCALL2ID did not execute")
H.near(sum_move(3, 4.5, 1000), 7500.0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum_const(1000), 4500.0)
H.near(sum_move(3, 4.5, 1000), 7500.0)
print("ffi/43_trace_ccall2id ok")
