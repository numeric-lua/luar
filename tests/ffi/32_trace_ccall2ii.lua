-- tests/ffi/32_trace_ccall2ii.lua -- two-integer-argument C calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/32_trace_ccall2ii: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/32_trace_ccall2ii: skipped (no native target)")
  return
end

ffi.cdef[[
int kill(int pid, int sig);
int getpid(void);
]]
local getok, kill = pcall(function () return ffi.C.kill end)
if not getok then
  print("ffi/32_trace_ccall2ii: skipped (no kill symbol)")
  return
end
local pid = ffi.C.getpid()

local N = 1000
jit.flush()
jit.opt.start("ir=1")
jit.on()

-- kill(pid, 0) only probes process existence; both arguments are MOVE/LOADI.
local function g (fn, p, n)
  local s = 0
  for _ = 1, n do s = s + fn(p, 0) + 1 end
  return s
end
H.eq(g(kill, pid, N), N)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "two-int C-call trace did not compile")
H.check(st.ir_executed >= 1, "two-int C-call trace did not execute")
H.eq(st.ir_call_exits, 0, "two-int C-call trace exited at the call")

-- Same Proto with a float pid: the entry integer guard must fall back to the
-- interpreter, which converts the argument, and no CALL exit is recorded.
local call_exits = st.ir_call_exits
H.eq(g(kill, pid + 0.0, N), N)
H.eq(jit.stats().ir_call_exits, call_exits,
     "float fallback reached the native two-int C-call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/32_trace_ccall2ii ok")
