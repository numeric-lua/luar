-- tests/ffi/85_int64_array_copy.lua -- raw int64 cdata array copy.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/85_int64_array_copy: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/85_int64_array_copy: skipped (no native target)")
  return
end

local n = 1001
local src = ffi.new("int64_t[?]", n)
local dst = ffi.new("int64_t[?]", n)
for i = 0, n - 1 do src[i] = i * 3 end
local function copy (m)
  for i = 0, m - 1 do dst[i] = src[i] end
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
copy(n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "int64 copy did not compile")
H.check(st.ir_executed >= 1, "int64 copy did not execute")
H.eq(st.ir_entry_failures, 0)
for i = 0, n - 1 do
  H.eq(tostring(dst[i]), tostring(src[i]), "int64 copy mismatch at " .. i)
end
H.eq(tostring(dst[1000]), "3000LL")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
for i = 0, n - 1 do dst[i] = ffi.cast("int64_t", 0) end
copy(n)
H.eq(tostring(dst[1000]), "3000LL")

jit.flush()
jit.opt.start("ir=0")
jit.on()
copy(n)
H.eq(tostring(dst[1000]), "3000LL")
print("ffi/85_int64_array_copy ok")
