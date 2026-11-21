-- tests/ffi/39_trace_cdata_struct_fields.lua -- safe 32-bit/float cdata
-- field traces; 64-bit reads stay interpreter-side.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/39_trace_cdata_struct_fields: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/39_trace_cdata_struct_fields: skipped (no native target)")
  return
end

ffi.cdef[[
struct luar_trace_IntPtr { int *p; };
struct luar_trace_Fields { int x; double y; long z; };
]]
local N = 1000

local ip = ffi.new("struct luar_trace_IntPtr")
local ia = ffi.new("int[?]", N)
for i = 0, N - 1 do ia[i] = i end
ip.p = ia

local function sum_int_ptr (s, n)
  local x = 0
  for i = 0, n - 1 do x = x + s.p[i] end
  return x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(sum_int_ptr(ip, N), 499500)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int-pointer trace did not compile")
H.check(st.ir_executed >= 1, "int-pointer trace did not execute")

local sf = ffi.new("struct luar_trace_Fields")
sf.x = 7
sf.y = 2.5
sf.z = 11
local function sum_int_field (s, n)
  local x = 0
  for _ = 1, n do x = x + s.x end
  return x
end
local function sum_double_field (s, n)
  local x = 0.0
  for _ = 1, n do x = x + s.y end
  return x
end
H.eq(sum_int_field(sf, N), 7000)
H.near(sum_double_field(sf, N), 2500.0)
H.check(jit.stats().ir_compiled >= 3, "struct fields did not compile")

-- 64-bit reads return cdata and must stay interpreter-side; the result is
-- still correct.
local lf = ffi.new("struct luar_trace_Fields")
lf.z = 1234567890123
local function sum_long_field (s, n)
  local x = 0
  for _ = 1, n do x = x + s.z end
  return x
end
H.eq(tonumber(sum_long_field(lf, 10)), 12345678901230)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/39_trace_cdata_struct_fields ok")
