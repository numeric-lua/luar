-- tests/ffi/81_int64_array_semantics.lua -- int64_t array reads must stay
-- cdata int64 and must not be consumed by the 32-bit CGETI path.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/81_int64_array_semantics: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/81_int64_array_semantics: skipped (no native target)")
  return
end

local a = ffi.new("int64_t[1001]")
for i = 0, 1000 do a[i] = i end
local function sum (n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end

jit.off()
local expected = sum(1001)
jit.on()
jit.flush()
jit.opt.start("ir=1")
local got = sum(1001)
H.eq(type(got), "cdata")
H.eq(tostring(got), tostring(expected))
H.eq(tostring(got), "500500LL")

jit.flush()
jit.opt.start("ir=0")
local plain = sum(1001)
H.eq(tostring(plain), "500500LL")
print("ffi/81_int64_array_semantics ok")
