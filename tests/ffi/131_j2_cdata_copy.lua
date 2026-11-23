-- ffi/131_j2_cdata_copy.lua -- default J2 bounded cdata array copies.
--
-- `a[i] = b[i]` over owned cdata arrays now runs through a bounds-checked
-- C helper.  It converts each source element to the destination element
-- type, follows the interpreter's FORLOOP key sequence for any nonzero
-- integer step, and leaves pointers and out-of-bounds runs to T0.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/131_j2_cdata_copy: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/131_j2_cdata_copy: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    if a ~= a or b ~= b then return false end
    return bits(a) == bits(b)
  end
  return a == b
end

local N = 257

local function make (ct, init)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = init(i) end
  return a
end

local function copy (dst, src, n)
  for i = 0, n - 1 do dst[i] = src[i] end
  return dst[0], dst[n // 2], dst[n - 1]
end

local function copy_step (dst, src, n)
  for i = 0, n - 1, 3 do dst[i] = src[i] end
  return dst[0], dst[1], dst[n - 2], dst[n - 1]
end

local function copy_down (dst, src, n)
  for i = n - 1, 0, -2 do dst[i] = src[i] end
  return dst[0], dst[1], dst[n - 2], dst[n - 1]
end

local function copy_ptr (dst, src, n)
  local p = ffi.cast("int32_t *", src)
  for i = 0, n - 1 do dst[i] = p[i] end
  return dst[0], dst[n - 1]
end

local function copy_oob (dst, src, n)
  for i = 0, n - 1 do dst[i] = src[i] end
  return dst[0]
end

local function run (name, dstct, srcct, srcinit, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(dstct, function () return 0 end),
                       make(srcct, srcinit), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(dstct, function () return 0 end),
                      make(srcct, srcinit), N, table.unpack(args)) }
  local hits = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.check(same(got[i], want[i]),
              name .. ": result " .. i .. " differs: " ..
              tostring(got[i]) .. " ~= " .. tostring(want[i]))
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  return hits
end

local ints = function (i) return i * 37 - 5000 end
local bytes = function (i) return i * 5 - 600 end
local ubytes = function (i) return i * 7 + 3 end
local doubles = function (i) return i * 0.25 - 3.5 end
local floats = function (i) return i * 0.5 - 1.25 end

H.check(run("int32 copy", "int32_t", "int32_t", ints, copy) > 0,
        "int32 copy did not use tint")
H.check(run("int32 -> double", "double", "int32_t", ints, copy) > 0,
        "int-to-double copy did not use tint")
H.check(run("double -> int32", "int32_t", "double", doubles, copy) > 0,
        "double-to-int copy did not use tint")
H.check(run("int8 -> int32", "int32_t", "int8_t", bytes, copy) > 0,
        "int8 widening copy did not use tint")
H.check(run("uint8 -> int32", "int32_t", "uint8_t", ubytes, copy) > 0,
        "uint8 widening copy did not use tint")
H.check(run("int32 -> int8", "int8_t", "int32_t", ints, copy) > 0,
        "int8 truncating copy did not use tint")
H.check(run("float -> double", "double", "float", floats, copy) > 0,
        "float-to-double copy did not use tint")
H.check(run("double -> float", "float", "double", doubles, copy) > 0,
        "double-to-float copy did not use tint")
H.check(run("strided copy", "int32_t", "int32_t", ints, copy_step) > 0,
        "strided copy did not use tint")
H.check(run("descending copy", "int32_t", "int32_t", ints, copy_down) > 0,
        "descending copy did not use tint")

-- Pointers have no bounded length; the helper must decline.
H.eq(run("pointer source fallback", "int32_t", "int32_t", ints, copy_ptr), 0,
     "a pointer source must not use the bounded helper")
run("out of bounds source", "int32_t", "int32_t", ints, copy_oob, N + 1)

-- A copy that aliases its destination is element-wise and safe.
do
  local function self_copy (a, n)
    for i = 0, n - 1 do a[i] = a[i] end
    return a[0], a[n // 2], a[n - 1]
  end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local a = make("int32_t", ints)
  local r1, r2, r3 = self_copy(a, N)
  H.eq(r1, ints(0), "self copy first")
  H.eq(r2, ints(N // 2), "self copy middle")
  H.eq(r3, ints(N - 1), "self copy last")
end

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/131_j2_cdata_copy ok")
