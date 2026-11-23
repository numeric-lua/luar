-- ffi/134_j2_cdata_pair_store.lua -- default J2 two-source cdata stores.
--
-- `a[i] = b[i] + c[i]` (and the subtract/multiply forms) over bounded
-- cdata arrays now run through a C helper that converts the computed value
-- to the destination element type.  Pointers, mixed table/cdata sources
-- and out-of-bounds runs stay on the interpreter.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/134_j2_cdata_pair_store: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/134_j2_cdata_pair_store: skipped (no native target)")
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

local function add (dst, b, c, n)
  for i = 0, n - 1 do dst[i] = b[i] + c[i] end
  return dst[0], dst[n // 2], dst[n - 1]
end

local function sub (dst, b, c, n)
  for i = 0, n - 1 do dst[i] = b[i] - c[i] end
  return dst[0], dst[n // 2], dst[n - 1]
end

local function mul (dst, b, c, n)
  for i = 0, n - 1 do dst[i] = b[i] * c[i] end
  return dst[0], dst[n // 2], dst[n - 1]
end

local function add_step (dst, b, c, n)
  for i = 0, n - 1, 3 do dst[i] = b[i] + c[i] end
  return dst[0], dst[1], dst[n - 2], dst[n - 1]
end

local function add_down (dst, b, c, n)
  for i = n - 1, 0, -2 do dst[i] = b[i] + c[i] end
  return dst[0], dst[1], dst[n - 2], dst[n - 1]
end

local function add_ptr (dst, b, c, n)
  local p = ffi.cast("int32_t *", b)
  for i = 0, n - 1 do dst[i] = p[i] + c[i] end
  return dst[0], dst[n - 1]
end

local function add_oob (dst, b, c, n)
  for i = 0, n - 1 do dst[i] = b[i] + c[i] end
  return dst[0]
end

local function run (name, dstct, bct, cct, binit, cinit, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(dstct, function () return 0 end),
                       make(bct, binit), make(cct, cinit), N,
                       table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(dstct, function () return 0 end),
                      make(bct, binit), make(cct, cinit), N,
                      table.unpack(args)) }
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
local ints2 = function (i) return i * 11 + 7 end
local doubles = function (i) return i * 0.25 - 3.5 end
local doubles2 = function (i) return i * 0.125 + 1.5 end
local floats = function (i) return i * 0.5 - 1.25 end
local bytes = function (i) return i * 5 - 600 end

H.check(run("int add", "int32_t", "int32_t", "int32_t", ints, ints2, add) > 0,
        "integer pair add did not use tint")
H.check(run("int sub", "int32_t", "int32_t", "int32_t", ints, ints2, sub) > 0,
        "integer pair subtract did not use tint")
H.check(run("int mul", "int32_t", "int32_t", "int32_t", ints, ints2, mul) > 0,
        "integer pair multiply did not use tint")
H.check(run("double add", "double", "double", "double", doubles, doubles2,
            add) > 0, "double pair add did not use tint")
H.check(run("double sub", "double", "double", "double", doubles, doubles2,
            sub) > 0, "double pair subtract did not use tint")
H.check(run("double mul", "double", "double", "double", doubles, doubles2,
            mul) > 0, "double pair multiply did not use tint")
H.check(run("int -> double", "double", "int32_t", "int32_t", ints, ints2,
            add) > 0, "int-to-double pair add did not use tint")
H.check(run("float add", "float", "float", "float", floats, floats, add) > 0,
        "float pair add did not use tint")
H.check(run("int8 sources", "int32_t", "int8_t", "int8_t", bytes, bytes,
            add) > 0, "int8-source pair add did not use tint")
H.check(run("same source", "int32_t", "int32_t", "int32_t", ints, ints,
            add) > 0, "same-source pair add did not use tint")
H.check(run("strided", "int32_t", "int32_t", "int32_t", ints, ints2,
            add_step) > 0, "strided pair store did not use tint")
H.check(run("descending", "int32_t", "int32_t", "int32_t", ints, ints2,
            add_down) > 0, "descending pair store did not use tint")

-- Pointers have no bounded length; the helper must decline.
H.eq(run("pointer fallback", "int32_t", "int32_t", "int32_t", ints, ints2,
         add_ptr), 0, "a pointer source must not use the bounded helper")
run("out of bounds", "int32_t", "int32_t", "int32_t", ints, ints2,
    add_oob, N + 1)

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/134_j2_cdata_pair_store ok")
