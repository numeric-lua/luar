-- tests/ffi/90_int64_sub.lua -- fused int64 decrement.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/90_int64_sub: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/90_int64_sub: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i + 5 end
local function dec (m)
  for i = 0, m - 1 do a[i] = a[i] - 1 end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
dec(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 sub did not compile")
H.check(st.ir_executed >= 1, "int64 sub did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "4LL")
H.eq(tostring(a[1000]), "1004LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
dec(n)
H.eq(tostring(a[1000]), "1003LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
dec(n)
H.eq(tostring(a[1000]), "1002LL")
print("ffi/90_int64_sub ok")
