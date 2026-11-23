-- ffi/135_j2_cdata_scalar_store.lua -- default J2 scaled cdata stores.
--
-- `a[i] = b[i] + v`, `- v`, `* v` and `/ v` over two bounded arrays now
-- run through a bounds-checked C helper.  The helper performs Lua's
-- int-wrap/float-promotion arithmetic on the source element and the scalar
-- first, then applies the FFI store conversion to the destination element
-- type, and keeps the last computed result in the bytecode temporary.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/135_j2_cdata_scalar_store: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/135_j2_cdata_scalar_store: skipped (no native target)")
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

local function add_var (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] + k end
  return d[0], d[n // 2], d[n - 1]
end

local function sub_var (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] - k end
  return d[0], d[n // 2], d[n - 1]
end

local function mul_var (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] * k end
  return d[0], d[n // 2], d[n - 1]
end

local function div_var (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] / k end
  return d[0], d[n // 2], d[n - 1]
end

local function add_imm (d, b, n)
  for i = 0, n - 1 do d[i] = b[i] + 7 end
  return d[0], d[n // 2], d[n - 1]
end

local function mul_k (d, b, n)
  for i = 0, n - 1 do d[i] = b[i] * 2.5 end
  return d[0], d[n // 2], d[n - 1]
end

local function step_var (d, b, n, k)
  for i = 0, n - 1, 3 do d[i] = b[i] + k end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function down_var (d, b, n, k)
  for i = n - 1, 0, -2 do d[i] = b[i] + k end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function ptr_src (d, b, n, k)
  local p = ffi.cast("int32_t *", b)
  for i = 0, n - 1 do d[i] = p[i] + k end
  return d[0], d[n - 1]
end

local function nonnumeric (d, b, n, k)
  for i = 0, n - 1 do d[i] = b[i] + k end
  return d[0]
end

local function run (name, dct, bct, init, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(dct, function () return 0 end),
                       make(bct, init), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(dct, function () return 0 end),
                      make(bct, init), N, table.unpack(args)) }
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
local doubles = function (i) return i * 0.25 - 3.5 end
local floats = function (i) return i * 0.5 - 1.25 end
local small = function (i) return i - 100 end

H.check(run("add int", "int32_t", "int32_t", ints, add_var, 5) > 0,
        "integer add did not use tint")
H.check(run("sub int", "int32_t", "int32_t", ints, sub_var, 5) > 0,
        "integer subtract did not use tint")
H.check(run("mul int", "int32_t", "int32_t", ints, mul_var, 3) > 0,
        "integer multiply did not use tint")
H.check(run("div int", "int32_t", "int32_t", ints, div_var, 2) > 0,
        "integer divide did not use tint")
H.check(run("add immediate", "int32_t", "int32_t", ints, add_imm) > 0,
        "ADDI source store did not use tint")
H.check(run("mul constant", "int32_t", "int32_t", ints, mul_k) > 0,
        "MULK source store did not use tint")
H.check(run("add double", "double", "double", doubles, add_var, 1.25) > 0,
        "double add did not use tint")
H.check(run("div double", "double", "double", doubles, div_var, 2) > 0,
        "double divide did not use tint")
H.check(run("int -> double", "double", "int32_t", ints, add_var, 0.5) > 0,
        "integer source to double destination did not use tint")
H.check(run("float source", "double", "float", floats, mul_var, 2.0) > 0,
        "float source did not use tint")
H.check(run("int8 destination", "int8_t", "int32_t", ints, add_var, 3) > 0,
        "int8 destination did not use tint")
H.check(run("uint8 destination", "uint8_t", "int32_t", ints, mul_var, 2) > 0,
        "uint8 destination did not use tint")
H.check(run("strided", "int32_t", "int32_t", ints, step_var, 7) > 0,
        "strided source store did not use tint")
H.check(run("descending", "int32_t", "int32_t", ints, down_var, 7) > 0,
        "descending source store did not use tint")

-- 8-byte cdata sources stay interpreted: an element read is cdata int64,
-- not a Lua integer, and the helper only models Lua numbers.
H.eq(run("int64 source fallback", "int32_t", "int64_t",
         function (i) return i + 1 end, add_var, 5), 0,
     "an int64 source must stay interpreted")

-- A pointer source has no bound and must not use the helper.
H.eq(run("pointer source fallback", "int32_t", "int32_t", ints,
         ptr_src, 7), 0, "a pointer source must not use the bounded helper")

-- A non-numeric scalar raises on the interpreter path with the same message.
run("non-numeric scalar", "int32_t", "int32_t", ints, nonnumeric, "x")
run("out of bounds", "int32_t", "int32_t", ints, add_var, N + 1)

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/135_j2_cdata_scalar_store ok")
