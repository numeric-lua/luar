-- tests/ffi/25_trace_ccall0.lua -- native zero-argument C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/25_trace_ccall0: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[int getpid(void);]]
local getpid = ffi.C.getpid
local pid = getpid()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function g (fn, n)
  local s = 0
  for i = 1, n do s = s + fn() end
  return s
end
H.eq(g(getpid, 1000), pid * 1000)
local st = jit.stats()
H.check(st.ir_executed >= 1, "native C-call trace did not execute")
H.eq(st.ir_call_exits, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/25_trace_ccall0 ok")
