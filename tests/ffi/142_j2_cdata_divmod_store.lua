-- ffi/142_j2_cdata_divmod_store.lua -- default J2 in-place and source
-- floor-division/modulo cdata stores.
--
-- `a[i] = a[i] // v`, `a[i] = a[i] % v`, `d[i] = b[i] // v` and
-- `d[i] = b[i] % v` now run through the J2 helpers.  Integer zero divisors
-- decline before any store so the interpreter raises the exact error;
-- float zero follows the non-raising Lua float path.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/142_j2_cdata_divmod_store: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/142_j2_cdata_divmod_store: skipped (no native target)")
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

local function upd_idiv (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] // k end
  return a[0], a[n // 2], a[n - 1]
end

local function upd_mod (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] % k end
  return a[0], a[n // 2], a[n - 1]
end

local function src_idiv (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] // k end
  return d[0], d[n // 2], d[n - 1]
end

local function src_mod (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] % k end
  return d[0], d[n // 2], d[n - 1]
end

local function src_idiv_imm (d, b, n)
  for i = 0, n - 1 do d[i] = b[i] // 3 end
  return d[0], d[n // 2], d[n - 1]
end

local function src_mod_k (d, b, n)
  for i = 0, n - 1 do d[i] = b[i] % 5 end
  return d[0], d[n // 2], d[n - 1]
end

local function upd_step (a, n, k)
  for i = 0, n - 1, 3 do a[i] = a[i] // k end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function src_down (d, b, n, k)
  for i = n - 1, 0, -2 do d[i] = b[i] % k end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function ptr_src (d, b, n, k)
  local p = ffi.cast("int32_t *", b)
  for i = 0, n - 1 do d[i] = p[i] // k end
  return d[0], d[n - 1]
end

local function run (name, dct, sct, init, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(dct, function () return 0 end),
                       make(sct, init), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(dct, function () return 0 end),
                      make(sct, init), N, table.unpack(args)) }
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

local function run_update (name, dct, sct, init, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(dct, init), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(dct, init), N, table.unpack(args)) }
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
local small = function (i) return i - 100 end
local doubles = function (i) return i * 0.25 - 3.5 end
local floats = function (i) return i * 0.5 - 1.25 end

H.check(run_update("update idiv", "int32_t", "int32_t", ints, upd_idiv, 7) > 0,
        "in-place idiv did not use tint")
H.check(run_update("update mod", "int32_t", "int32_t", ints, upd_mod, 7) > 0,
        "in-place mod did not use tint")
H.check(run_update("update negative idiv", "int32_t", "int32_t", ints,
            upd_idiv, -3) > 0, "negative in-place idiv did not use tint")
H.check(run_update("update negative mod", "int32_t", "int32_t", ints,
            upd_mod, -3) > 0, "negative in-place mod did not use tint")
H.check(run_update("update double idiv", "double", "double", doubles,
            upd_idiv, 2) > 0, "double in-place idiv did not use tint")
H.check(run_update("update double mod", "double", "double", doubles,
            upd_mod, 2) > 0, "double in-place mod did not use tint")
H.check(run_update("update float element", "float", "float", floats,
            upd_idiv, 1.5) > 0, "float in-place idiv did not use tint")
H.check(run_update("update int8 wrap", "int8_t", "int8_t", small,
            upd_idiv, 3) > 0, "int8 in-place idiv did not use tint")
H.check(run_update("update strided", "int32_t", "int32_t", ints,
            upd_step, 4) > 0, "strided in-place idiv did not use tint")

H.check(run("source idiv", "int32_t", "int32_t", ints,
            src_idiv, 7) > 0, "source idiv did not use tint")
H.check(run("source mod", "int32_t", "int32_t", ints,
            src_mod, 7) > 0, "source mod did not use tint")
H.check(run("source idiv immediate", "int32_t", "int32_t", ints,
            src_idiv_imm) > 0, "immediate source idiv did not use tint")
H.check(run("source mod constant", "int32_t", "int32_t", ints,
            src_mod_k) > 0, "constant source mod did not use tint")
H.check(run("source int -> double", "double", "int32_t", ints,
            src_idiv, 2) > 0, "int-to-double source idiv did not use tint")
H.check(run("source descending", "int32_t", "int32_t", ints,
            src_down, 5) > 0, "descending source mod did not use tint")

-- Float zero is not an error and stays in the helper.
H.check(run_update("update float zero", "int32_t", "int32_t", ints,
            upd_idiv, 0.0) > 0, "float-zero in-place idiv did not use tint")
H.check(run("source float zero", "int32_t", "int32_t", ints,
            src_mod, 0.0) > 0, "float-zero source mod did not use tint")

-- Integer zero raises; the helper declines before storing anything.
H.eq(run_update("update idiv by zero", "int32_t", "int32_t", ints,
         upd_idiv, 0), 0,
     "in-place idiv by zero must stay interpreted")
H.eq(run_update("update mod by zero", "int32_t", "int32_t", ints,
         upd_mod, 0), 0,
     "in-place mod by zero must stay interpreted")
H.eq(run("source idiv by zero", "int32_t", "int32_t", ints,
         src_idiv, 0), 0,
     "source idiv by zero must stay interpreted")
H.eq(run("source mod by zero", "int32_t", "int32_t", ints,
         src_mod, 0), 0,
     "source mod by zero must stay interpreted")

-- 8-byte integer elements and pointers stay interpreted.
H.eq(run("int64 fallback", "int64_t", "int64_t",
         function (i) return i + 1 end, upd_idiv, 3), 0,
     "an int64 element must stay interpreted")
H.eq(run("pointer fallback", "int32_t", "int32_t", ints,
         ptr_src, 3), 0,
     "a pointer source must not use the bounded helper")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/142_j2_cdata_divmod_store ok")
