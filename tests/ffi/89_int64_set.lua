-- tests/ffi/89_int64_set.lua -- fused int64 assignment from Lua integers.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/89_int64_set: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/89_int64_set: skipped (no native target)")
  return
end

local n = 1001
local a = ffi.new("int64_t[?]", n)
local function fill (m)
  for i = 0, m - 1 do a[i] = i * 2 end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
fill(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 set did not compile")
H.check(st.ir_executed >= 1, "int64 set did not execute")
H.eq(st.ir_entry_failures, 0)
H.eq(tostring(a[0]), "0LL")
H.eq(tostring(a[500]), "1000LL")
H.eq(tostring(a[1000]), "2000LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
fill(n)
H.eq(tostring(a[1000]), "2000LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
fill(n)
H.eq(tostring(a[1000]), "2000LL")
print("ffi/89_int64_set ok")
