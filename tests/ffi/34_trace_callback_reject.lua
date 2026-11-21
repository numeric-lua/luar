-- tests/ffi/34_trace_callback_reject.lua -- callbacks must never enter J4
-- as native C calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/34_trace_callback_reject: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/34_trace_callback_reject: skipped (no native target)")
  return
end

ffi.cdef[[int cb0(void);]]
local cb = ffi.callback("int(void)", function () return 7 end)

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function loop (fn, n)
  local s = 0
  for _ = 1, n do s = s + fn() end
  return s
end
-- A CALL-exit prefix may compile; what matters is that the callback body is
-- never entered from generated code and the result stays correct.
H.eq(loop(cb, 1000), 7000)

-- A regular function with the same signature still traces.
ffi.cdef[[int getpid(void);]]
if ffi.C.getpid then
  local before = jit.stats().ir_compiled
  local function normal (fn, n)
    local s = 0
    for _ = 1, n do s = s + fn() end
    return s
  end
  local pid = ffi.C.getpid()
  H.eq(normal(ffi.C.getpid, 1000), pid * 1000)
  H.check(jit.stats().ir_compiled > before, "normal C call stopped tracing")
end

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/34_trace_callback_reject ok")
