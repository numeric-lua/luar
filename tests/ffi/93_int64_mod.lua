-- tests/ffi/93_int64_mod.lua -- fused int64 modulo by constant.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/93_int64_mod: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/93_int64_mod: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i - 500 end
local function mod3 (m)
  for i = 0, m - 1 do a[i] = a[i] % 3 end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
mod3(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 mod did not compile")
H.check(st.ir_executed >= 1, "int64 mod did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "-2LL")
H.eq(tostring(a[500]), "0LL")
H.eq(tostring(a[1000]), "2LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
mod3(n)
H.eq(tostring(a[0]), "-2LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
mod3(n)
H.eq(tostring(a[0]), "-2LL")
print("ffi/93_int64_mod ok")
