-- ffi/152_j2_cdata_upval_update.lua -- J2 in-place updates of bounded
-- cdata arrays held in closure upvalues.
--
-- The compiler loads the upvalue twice per iteration (destination and
-- source).  The native helper re-resolves it on every entry, preserves the
-- GETUPVAL destination register and the arithmetic temporary, and declines
-- a rebound non-array so the interpreter reports the original error.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/152_j2_cdata_upval_update: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/152_j2_cdata_upval_update: skipped (no native target)")
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

local function make_op (op)
  local a
  local function set (v) a = v end
  local f
  if op == "add" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] + v end end
  elseif op == "sub" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] - v end end
  elseif op == "mul" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] * v end end
  elseif op == "div" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] / v end end
  elseif op == "idiv" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] // v end end
  elseif op == "mod" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] % v end end
  elseif op == "band" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] & v end end
  elseif op == "bor" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] | v end end
  elseif op == "bxor" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] ~ v end end
  elseif op == "shl" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] << v end end
  elseif op == "shr" then
    f = function (n, v) for i = 0, n - 1 do a[i] = a[i] >> v end end
  elseif op == "bnot" then
    f = function (n) for i = 0, n - 1 do a[i] = ~a[i] end end
  elseif op == "unm" then
    f = function (n) for i = 0, n - 1 do a[i] = -a[i] end end
  elseif op == "addk" then
    f = function (n) for i = 0, n - 1 do a[i] = a[i] + 3 end end
  elseif op == "mulkc" then
    f = function (n) for i = 0, n - 1 do a[i] = a[i] * 2.5 end end
  elseif op == "shlk" then
    f = function (n) for i = 0, n - 1 do a[i] = a[i] << 2 end end
  else
    error("unknown op " .. op)
  end
  return set, f
end

local function runop (name, op, ct, fill, v)
  local set, f = make_op(op)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  local wa = make(ct, fill)
  set(wa)
  local okw, ew = pcall(function () return f(N, v) end)
  local wantb = bytes(wa, ct)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local ba = make(ct, fill)
  set(ba)
  local before = jit.stats().tint_hits
  local okj, ej = pcall(function () return f(N, v) end)
  local hits = jit.stats().tint_hits - before
  H.eq(okj, okw, name .. ": status")
  if not okw then
    H.eq(tostring(ej), tostring(ew), name .. ": error")
  end
  H.eq(bytes(ba, ct), wantb, name .. ": bytes")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
  return ba, set, f
end

local function seed (i) return (i * 37) % 101 - 50 end
local function useed (i) return i * 13 - 7 end

runop("int32 add", "add", "int32_t", seed, 5)
runop("uint16 sub", "sub", "uint16_t", useed, 3)
runop("int32 mul", "mul", "int32_t", seed, -2)
runop("double div", "div", "double", function (i) return i / 3 end, 2)
runop("int32 idiv", "idiv", "int32_t", seed, 3)
runop("int32 mod", "mod", "int32_t", seed, 7)
runop("int32 band", "band", "int32_t", seed, 0x55)
runop("uint32 bor", "bor", "uint32_t", function (i) return i * 3 end, 0x100)
runop("int16 bxor", "bxor", "int16_t", seed, -17)
runop("uint8 shl", "shl", "uint8_t", function (i) return i end, 3)
runop("int32 shr", "shr", "int32_t", seed, 2)
runop("int32 bnot", "bnot", "int32_t", seed)
runop("int32 unm", "unm", "int32_t", seed)
runop("int32 addk", "addk", "int32_t", seed)
runop("double mulk", "mulkc", "double", function (i) return i / 5 end)
runop("int32 shlk", "shlk", "int32_t", seed)

-- Rebinding the upvalue after compilation is observed: the next call
-- updates the new array, not the one used when the loop was compiled.
do
  local set, f = make_op("add")
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local a = make("int32_t", function (i) return i end)
  local b = make("int32_t", function (i) return 1000 - i end)
  set(a)
  f(N, 1)
  set(b)
  f(N, 2)
  for i = 0, N - 1 do
    H.eq(b[i], (1000 - i) + 2, "rebound upvalue update at " .. i)
  end
end

-- A rebound non-array must fall back to the interpreter and raise the
-- exact arithmetic error, not write through a stale array pointer.
do
  local set, f = make_op("add")
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(make("int32_t", seed))
  f(N, 1)
  set({})
  local ok2, err = pcall(function () f(N, 1) end)
  H.check(not ok2, "non-array upvalue must raise")
  H.check(tostring(err):find("arithmetic", 1, true) ~= nil,
          "unexpected non-array error: " .. tostring(err))
end

-- Rebinding to a float array makes a bitwise update raise the exact
-- integer-conversion error through the interpreter fallback.
do
  local set, f = make_op("band")
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(make("int32_t", seed))
  f(N, 0x55)
  set(make("double", function (i) return i + 0.5 end))
  local ok2, err = pcall(function () f(N, 0x55) end)
  H.check(not ok2, "float bitwise upvalue must raise")
  H.check(tostring(err):find("integer", 1, true) ~= nil,
          "unexpected float bitwise error: " .. tostring(err))
end

jit.flush()
print("ffi/152_j2_cdata_upval_update ok")
