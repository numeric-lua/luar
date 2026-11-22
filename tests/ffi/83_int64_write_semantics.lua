-- tests/ffi/83_int64_write_semantics.lua -- int64_t array writes must keep
-- interpreter semantics and must not be routed through the 32-bit helper.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/83_int64_write_semantics: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/83_int64_write_semantics: skipped (no native target)")
  return
end

local function fill (n)
  local a = ffi.new("int64_t[?]", n)
  for i = 0, n - 1 do a[i] = i * 3 end
  return a
end

jit.off()
local expected = fill(1001)
jit.on()
jit.flush()
jit.opt.start("ir=1")
local got
for _ = 1, 3 do got = fill(1001) end
for i = 0, 1000 do
  H.eq(tostring(got[i]), tostring(expected[i]), "int64 write mismatch at " .. i)
end
H.eq(tostring(got[1000]), "3000LL")

jit.flush()
jit.opt.start("ir=0")
local plain = fill(1001)
H.eq(tostring(plain[1000]), "3000LL")
print("ffi/83_int64_write_semantics ok")
