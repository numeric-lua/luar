-- tests/ffi/82_cdata_int_widths.lua -- J4 must match interpreter for
-- 8/16/32-bit signed and unsigned cdata integer arrays.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/82_cdata_int_widths: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/82_cdata_int_widths: skipped (no native target)")
  return
end

local function check (ty, fill, n)
  local a = ffi.new(ty .. "[?]", n + 1)
  for i = 0, n do a[i] = fill(i) end
  local function sum (k)
    local s = 0
    for i = 0, k - 1 do s = s + a[i] end
    return s
  end
  jit.off()
  local expected = sum(n + 1)
  jit.on()
  jit.flush()
  jit.opt.start("ir=1")
  local got
  for _ = 1, 3 do got = sum(n + 1) end
  H.eq(tostring(got), tostring(expected), ty .. " JIT/interpreter mismatch")
  return got
end

check("int8_t", function (i) return (i % 251) - 125 end, 500)
check("int16_t", function (i) return (i % 60000) - 30000 end, 500)
check("int32_t", function (i) return (i * 65537) - 1000000 end, 500)
check("uint32_t", function (i) return (i * 65537) % 4294967296 end, 500)

jit.flush()
jit.opt.start("ir=0")
print("ffi/82_cdata_int_widths ok")
