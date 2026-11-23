-- ffi/150_j2_enum_array.lua -- J2 reductions, stores and in-place updates
-- over bounded enum arrays.
--
-- Lua exposes an enum element through its unsigned 32-bit numeric view, so
-- the native helper loads it as unsigned (including values with the high
-- bit set) and writes it through the same CT_ENUM conversion as
-- ffi_convert.  Register arrays, upvalue arrays and every arithmetic form
-- share the existing cdata helpers.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/150_j2_enum_array: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/150_j2_enum_array: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum J2Enum { J2_ZERO = 0, J2_ONE = 1, J2_NEG = -1,
                     J2_BIG = 0x7fffffff } J2Enum;
]]
H.eq(ffi.sizeof("J2Enum"), 4, "enum size")

local N = 257

local function make (fill)
  local a = ffi.new("J2Enum[?]", N)
  for i = 0, N - 1 do a[i] = fill(i) end
  return a
end

local function bytes (a)
  return ffi.string(ffi.cast("uint8_t *", a), N * 4)
end

local function run (name, f, fill, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  local a = make(fill)
  local want = { pcall(f, a, N, table.unpack(args)) }
  local want_bytes = bytes(a)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local b = make(fill)
  local before = jit.stats().tint_hits
  local got = { pcall(f, b, N, table.unpack(args)) }
  local hits = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.eq(tostring(got[i]), tostring(want[i]), name .. ": result " .. i)
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error")
  end
  H.eq(bytes(b), want_bytes, name .. ": bytes")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
end

-- Values include -1, which reads back as 0xffffffff, plus a high-bit value.
local function seed (i)
  local v = (i * 37) % 11 - 5
  if i == 3 then v = -1 end
  if i == 7 then v = 0x7fffffff end
  return v
end

run("enum sum", function (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end, seed)

-- The upvalue form needs its own fixture so the array is rebound per run.
do
  local a
  local function set (v) a = v end
  local function f (n)
    local s = 0
    for i = 0, n - 1 do s = s + a[i] end
    return s
  end
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  set(make(seed))
  local want = f(N)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(make(seed))
  local before = jit.stats().tint_hits
  local got = f(N)
  H.eq(got, want, "enum upvalue sum")
  H.check(jit.stats().tint_hits - before > 0, "upvalue enum sum intrinsic")
end

run("enum add", function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] + v end
  return a[0], a[N // 2], a[N - 1]
end, seed, 3)

run("enum float add", function (a, n)
  for i = 0, n - 1 do a[i] = a[i] + 0.75 end
  return a[0], a[N // 2], a[N - 1]
end, seed)

run("enum band", function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] & v end
  return a[0], a[N // 2], a[N - 1]
end, seed, 0x0f)

run("enum copy", function (d, n)
  local src = ffi.new("int32_t[?]", n)
  for i = 0, n - 1 do src[i] = i * 13 - 100 end
  for i = 0, n - 1 do d[i] = src[i] end
  return d[0], d[N // 2], d[N - 1]
end, function () return 0 end)

run("enum value store", function (a, n, v)
  for i = 0, n - 1 do a[i] = v end
  return a[0], a[N - 1]
end, function () return 0 end, -1)

run("enum index store", function (a, n)
  for i = 0, n - 1 do a[i] = i end
  return a[0], a[N - 1]
end, function () return 0 end)

jit.flush()
print("ffi/150_j2_enum_array ok")
