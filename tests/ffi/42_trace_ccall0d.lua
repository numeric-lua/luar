-- tests/ffi/42_trace_ccall0d.lua -- zero-argument double-return C calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/42_trace_ccall0d: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/42_trace_ccall0d: skipped (no native target)")
  return
end

local S = dofile("tests/ffi/support.lua")
local helper = S.helper()
ffi.cdef[[double double_ret(void);]]
local f = helper.double_ret

local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f() end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 2500.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CCALL0D did not compile")
H.check(st.ir_executed >= 1, "CCALL0D did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 2500.0)
print("ffi/42_trace_ccall0d ok")
