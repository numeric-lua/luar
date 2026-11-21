-- tests/ffi/38_trace_cdata_array_upvalue.lua -- cdata array upvalues feed
-- native float32/double calls in J4.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/38_trace_cdata_array_upvalue: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/38_trace_cdata_array_upvalue: skipped (no native target)")
  return
end

ffi.cdef[[
float sqrtf(float);
double fabs(double);
]]
local sqrtf, fabs = ffi.C.sqrtf, ffi.C.fabs
local N = 1000
local arr = ffi.new("double[?]", N)
for i = 0, N - 1 do arr[i] = 2.0 end
local root2 = sqrtf(2.0)

local function sum_sqrt (n)
  local s = 0.0
  for i = 0, n - 1 do s = s + sqrtf(arr[i]) end
  return s
end
local function sum_abs (n)
  local s = 0.0
  for i = 0, n - 1 do s = s + fabs(arr[i]) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(sum_sqrt(N), 1414.2135381698608)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "cdata array upvalue sqrtf did not compile")
H.check(st.ir_executed >= 1, "cdata array upvalue sqrtf did not execute")
H.eq(st.ir_call_exits, 0, "cdata array upvalue sqrtf exited at the call")

H.near(sum_abs(N), 2000.0)
H.check(jit.stats().ir_executed >= 1, "cdata array upvalue fabs did not execute")

-- Replacing the upvalue must exit through the cdata identity guard.
arr = { [0] = 2.0, [1] = 2.0, [2] = 2.0, [3] = 2.0 }
H.near(sum_sqrt(4), 4 * root2)
H.near(sum_abs(4), 8.0)

-- The same Proto with an integer cdata array must fall back through the
-- cdata identity guard; the interpreter converts ints to float.
local ia = ffi.new("int[?]", N)
for i = 0, N - 1 do ia[i] = i % 4 end
local ce = jit.stats().ir_call_exits
arr = ia
H.near(sum_sqrt(N), 250 * (0 + 1 + math.sqrt(2) + math.sqrt(3)), 1e-3)
H.eq(jit.stats().ir_call_exits, ce,
     "int-array fallback reached the float32 C-call op")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/38_trace_cdata_array_upvalue ok")
