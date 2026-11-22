-- tests/ffi/86_int64_copy_pointer.lua -- pointer-source and self int64
-- cdata copies.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/86_int64_copy_pointer: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/86_int64_copy_pointer: skipped (no native target)")
  return
end

local n = 101
local a = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do a[i] = i end
local p = ffi.cast("int64_t*", a)
local function from_ptr (m)
  for i = 0, m - 1 do a[i] = p[i] end
end
local function self_copy (m)
  for i = 0, m - 1 do a[i] = a[i] end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
from_ptr(n)
self_copy(n)
local st = jit.stats()
H.check(st.ir_compiled >= 2, "pointer/self copy did not compile")
H.check(st.ir_executed >= 2, "pointer/self copy did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[100]), "100LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
from_ptr(n)
self_copy(n)
H.eq(tostring(a[100]), "100LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
from_ptr(n)
self_copy(n)
H.eq(tostring(a[100]), "100LL")
print("ffi/86_int64_copy_pointer ok")
