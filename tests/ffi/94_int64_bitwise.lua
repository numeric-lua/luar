-- tests/ffi/94_int64_bitwise.lua -- fused int64 bitwise ops by constant.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/94_int64_bitwise: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/94_int64_bitwise: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
local ref = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i; ref[i] = ((i & 255) | 256) ~ 3 end
local function ops (m)
  for i = 0, m - 1 do
    a[i] = a[i] & 255
    a[i] = a[i] | 256
    a[i] = a[i] ~ 3
  end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
ops(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 bitops did not compile")
H.check(st.ir_executed >= 1, "int64 bitops did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "259LL")
H.eq(tostring(a[1000]), "491LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
for i = 0, n - 1 do a[i] = i end
ops(n)
for i = 0, n - 1 do H.eq(tostring(a[i]), tostring(ref[i])) end

jit.flush()
jit.opt.start("ir=0")
jit.on()
for i = 0, n - 1 do a[i] = i end
ops(n)
H.eq(tostring(a[1000]), "491LL")
print("ffi/94_int64_bitwise ok")
