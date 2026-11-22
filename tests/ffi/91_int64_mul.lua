-- tests/ffi/91_int64_mul.lua -- fused int64 multiply by constant.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/91_int64_mul: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/91_int64_mul: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i + 1 end
local function mul (m)
  for i = 0, m - 1 do a[i] = a[i] * 2 end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
mul(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 mul did not compile")
H.check(st.ir_executed >= 1, "int64 mul did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "2LL")
H.eq(tostring(a[1000]), "2002LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
mul(n)
H.eq(tostring(a[1000]), "4004LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
mul(n)
H.eq(tostring(a[1000]), "8008LL")
print("ffi/91_int64_mul ok")
