-- tests/ffi/64_trace_faddi_2ff.lua -- FADDI arguments to
-- float(float,float)->float native calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/64_trace_faddi_2ff: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/64_trace_faddi_2ff: skipped (no native target)")
  return
end

ffi.cdef[[float hypotf(float, float);]]
local f = ffi.C.hypotf
local v = 2.5
local function sum (n)
  local s = 0.0
  for _ = 1, n do s = s + f(v + 1, v + 2) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum(1000), 5700.877, 0.01)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "FADDI 2FF did not compile")
H.check(st.ir_executed >= 1, "FADDI 2FF did not execute")
H.eq(st.ir_entry_failures, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
H.near(sum(1000), 5700.877, 0.01)
print("ffi/64_trace_faddi_2ff ok")
