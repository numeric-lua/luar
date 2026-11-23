-- ffi/132_j2_cdata_update.lua -- default J2 in-place cdata arithmetic.
--
-- `a[i] = a[i] + v`, `- v`, `* v` and `/ v` over owned cdata arrays now
-- run through a bounds-checked C helper.  The helper performs Lua's
-- int-wrap/float-promotion arithmetic first and then applies the C store
-- conversion, so integer and floating element arrays both match T0.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/132_j2_cdata_update: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/132_j2_cdata_update: skipped (no native target)")
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

local function add_var (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] + k end
  return a[0], a[n // 2], a[n - 1]
end

local function sub_var (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] - k end
  return a[0], a[n // 2], a[n - 1]
end

local function mul_var (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] * k end
  return a[0], a[n // 2], a[n - 1]
end

local function div_var (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] / k end
  return a[0], a[n // 2], a[n - 1]
end

local function add_imm (a, n)
  for i = 0, n - 1 do a[i] = a[i] + 1 end
  return a[0], a[n // 2], a[n - 1]
end

local function mul_k (a, n)
  for i = 0, n - 1 do a[i] = a[i] * 3 end
  return a[0], a[n // 2], a[n - 1]
end

local function div_k (a, n)
  for i = 0, n - 1 do a[i] = a[i] / 2 end
  return a[0], a[n // 2], a[n - 1]
end

local function update_step (a, n, k)
  for i = 0, n - 1, 3 do a[i] = a[i] + k end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function update_down (a, n, k)
  for i = n - 1, 0, -2 do a[i] = a[i] - k end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function update_ptr (a, n, k)
  local p = ffi.cast("int32_t *", a)
  for i = 0, n - 1 do p[i] = p[i] + k end
  return a[0], a[n - 1]
end

local function update_oob (a, n)
  for i = 0, n - 1 do a[i] = a[i] + 1 end
  return a[0]
end

local function run (name, ct, init, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(ct, init), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(ct, init), N, table.unpack(args)) }
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
local big = function (i) return math.maxinteger - i end
local doubles = function (i) return i * 0.25 - 3.5 end
local floats = function (i) return i * 0.5 - 1.25 end
local small = function (i) return i - 100 end

H.check(run("add int", "int32_t", ints, add_var, 5) > 0,
        "integer add did not use tint")
H.check(run("sub int", "int32_t", ints, sub_var, 5) > 0,
        "integer subtract did not use tint")
H.check(run("mul int", "int32_t", ints, mul_var, 3) > 0,
        "integer multiply did not use tint")
H.check(run("div int", "int32_t", ints, div_var, 2) > 0,
        "integer divide did not use tint")
H.check(run("add immediate", "int32_t", ints, add_imm) > 0,
        "ADDI update did not use tint")
H.check(run("mul constant", "int32_t", ints, mul_k) > 0,
        "MULK update did not use tint")
H.check(run("div constant", "int32_t", ints, div_k) > 0,
        "DIVK update did not use tint")
H.check(run("add int64 wrap", "int64_t", big, add_var, 10) == 0,
        "int64 cdata updates must stay interpreted (element is cdata)")
H.check(run("add double", "double", doubles, add_var, 1.25) > 0,
        "double add did not use tint")
H.check(run("sub double", "double", doubles, sub_var, 0.5) > 0,
        "double subtract did not use tint")
H.check(run("mul double", "double", doubles, mul_var, 1.5) > 0,
        "double multiply did not use tint")
H.check(run("div double", "double", doubles, div_var, 2) > 0,
        "double divide did not use tint")
H.check(run("int -> double", "double", ints, add_var, 0.5) > 0,
        "integer element to double update did not use tint")
H.check(run("float element", "float", floats, mul_var, 2.0) > 0,
        "float element update did not use tint")
H.check(run("int8 wrap", "int8_t", small, add_var, 100) > 0,
        "int8 wrapping update did not use tint")
H.check(run("uint8 wrap", "uint8_t", small, sub_var, 100) > 0,
        "uint8 wrapping update did not use tint")
H.check(run("strided update", "int32_t", ints, update_step, 7) > 0,
        "strided update did not use tint")
H.check(run("descending update", "int32_t", ints, update_down, 7) > 0,
        "descending update did not use tint")

-- Pointers have no bounded length; the helper must decline.
H.eq(run("pointer fallback", "int32_t", ints, update_ptr, 7), 0,
     "a pointer update must not use the bounded helper")
run("out of bounds", "int32_t", ints, update_oob, N + 1)

-- A non-numeric scalar keeps the loop on the interpreter, where it raises
-- exactly the same error.
run("non-numeric scalar", "int32_t", ints, add_var, "x")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/132_j2_cdata_update ok")
