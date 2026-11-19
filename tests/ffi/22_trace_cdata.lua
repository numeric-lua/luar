-- tests/ffi/22_trace_cdata.lua -- J4 can read integer cdata array cells.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/22_trace_cdata: skipped (FFI unavailable)")
  return
end
if jit.status() ~= true then
  print("ffi/22_trace_cdata: skipped (no native target)")
  return
end

local a = ffi.new("int[?]", 1001)
for i = 0, 1000 do a[i] = i end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sum (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] end
  return s
end
assert(sum(a, 1000) == 500500)
assert(jit.stats().ir_executed >= 1)

-- an unsupported float cdata array must stay on the interpreter
local d = ffi.new("double[?]", 3)
d[1] = 2.5
assert(sum(d, 2) == 2.5)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/22_trace_cdata ok")
