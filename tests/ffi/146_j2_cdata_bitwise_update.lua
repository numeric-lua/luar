-- ffi/146_j2_cdata_bitwise_update.lua -- default J2 in-place integer
-- bitwise and unary cdata updates.
--
-- `a[i] = a[i] & v`, `|`, `~`, `<<`, `>>`, constant counts and unary
-- `~a[i]`/`-a[i]` run through the cdata update helper.  Lua's integer
-- conversion, wrap and shift rules are preserved: negative counts shift the
-- other way, |count| >= 64 yields zero, and a non-integral count falls back
-- to the interpreter so it raises the exact error.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/146_j2_cdata_bitwise_update: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/146_j2_cdata_bitwise_update: skipped (no native target)")
  return
end

local N = 257

local function make (ct, fill)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = fill(i) end
  return a
end

local function bytes (a, ct)
  return ffi.string(ffi.cast("uint8_t *", a), N * ffi.sizeof(ct))
end

local function run (name, ct, fill, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  local a = make(ct, fill)
  local want = { pcall(f, a, N, table.unpack(args)) }
  local want_bytes = bytes(a, ct)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local b = make(ct, fill)
  local before = jit.stats().tint_hits
  local got = { pcall(f, b, N, table.unpack(args)) }
  local hits = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.eq(tostring(got[i]), tostring(want[i]), name .. ": result " .. i)
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  H.eq(bytes(b, ct), want_bytes, name .. ": array bytes")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
end

local function seed (i) return i * 37 - 5000 end

run("int32 band var", "int32_t", seed, function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] & v end
  return a[0], a[N // 2], a[N - 1]
end, 0x55)
run("uint32 bor var", "uint32_t", function (i) return i * 13 end,
    function (a, n, v)
      for i = 0, n - 1 do a[i] = a[i] | v end
      return a[0], a[N // 2], a[N - 1]
    end, 0x100)
run("int16 bxor var", "int16_t", seed, function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] ~ v end
  return a[0], a[N // 2], a[N - 1]
end, -17)
run("uint8 band k", "uint8_t", function (i) return i * 3 end,
    function (a, n)
      for i = 0, n - 1 do a[i] = a[i] & 0x0f end
      return a[0], a[N // 2], a[N - 1]
    end)
run("int8 bor k", "int8_t", seed, function (a, n)
  for i = 0, n - 1 do a[i] = a[i] | 0x30 end
  return a[0], a[N // 2], a[N - 1]
end)
run("int32 bxor k", "int32_t", seed, function (a, n)
  for i = 0, n - 1 do a[i] = a[i] ~ 0x0f0f end
  return a[0], a[N // 2], a[N - 1]
end)
run("int32 shl var", "int32_t", seed, function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] << v end
  return a[0], a[N // 2], a[N - 1]
end, 5)
run("uint32 shr var", "uint32_t", function (i) return i * 0x10203 end,
    function (a, n, v)
      for i = 0, n - 1 do a[i] = a[i] >> v end
      return a[0], a[N // 2], a[N - 1]
    end, 7)
run("int32 shl negative", "int32_t", seed, function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] << v end
  return a[0], a[N // 2], a[N - 1]
end, -4)
run("int32 shr 64", "int32_t", seed, function (a, n, v)
  for i = 0, n - 1 do a[i] = a[i] >> v end
  return a[0], a[N // 2], a[N - 1]
end, 64)
run("int32 shl const", "int32_t", seed, function (a, n)
  for i = 0, n - 1 do a[i] = a[i] << 3 end
  return a[0], a[N // 2], a[N - 1]
end)
run("int32 shr const", "int32_t", seed, function (a, n)
  for i = 0, n - 1 do a[i] = a[i] >> 3 end
  return a[0], a[N // 2], a[N - 1]
end)
run("int32 bnot", "int32_t", seed, function (a, n)
  for i = 0, n - 1 do a[i] = ~a[i] end
  return a[0], a[N // 2], a[N - 1]
end)
run("int32 unary minus", "int32_t", seed, function (a, n)
  for i = 0, n - 1 do a[i] = -a[i] end
  return a[0], a[N // 2], a[N - 1]
end)
run("int16 strided band", "int16_t", seed, function (a, n, v)
  for i = 0, n - 1, 3 do a[i] = a[i] & v end
  return a[0], a[1], a[N - 2], a[N - 1]
end, 0x3f)
run("int32 descending shr", "int32_t", function (i) return i * 9 end,
    function (a, n, v)
      for i = n - 1, 0, -2 do a[i] = a[i] >> v end
      return a[0], a[1], a[N - 2], a[N - 1]
    end, 2)

-- A non-integral bitwise operand must fall back and raise the interpreter's
-- exact error rather than silently truncating.
do
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local a = make("int32_t", seed)
  local ok2, err = pcall(function ()
    for i = 0, N - 1 do a[i] = a[i] & 1.5 end
  end)
  H.check(not ok2, "fractional bitwise operand should raise")
  H.check(tostring(err):find("integer representation", 1, true) ~= nil,
          "unexpected fractional-bitwise error: " .. tostring(err))
end
