-- tests/ffi/92_int64_div.lua -- fused int64 floor division by constant.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/92_int64_div: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/92_int64_div: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i - 500 end
local function div2 (m)
  for i = 0, m - 1 do a[i] = a[i] // 2 end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
div2(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 div did not compile")
H.check(st.ir_executed >= 1, "int64 div did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "-250LL")
H.eq(tostring(a[500]), "0LL")
H.eq(tostring(a[1000]), "250LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
div2(n)
H.eq(tostring(a[0]), "-125LL")
H.eq(tostring(a[1000]), "125LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
div2(n)
H.eq(tostring(a[0]), "-62LL")
H.eq(tostring(a[1000]), "62LL")
print("ffi/92_int64_div ok")
