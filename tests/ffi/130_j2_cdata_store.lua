-- ffi/130_j2_cdata_store.lua -- default J2 bounded cdata array stores.
--
-- `a[i] = value`, `a[i] = i` and `a[i] = constant` over an owned cdata
-- array now run through a bounds-checked C helper.  The helper must apply
-- the FFI store conversions (integer truncation and LuaJIT's out-of-range
-- double sentinel), follow the interpreter's FORLOOP sequence for any
-- nonzero integer step, and leave pointers and out-of-bounds runs to the
-- interpreter.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/130_j2_cdata_store: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/130_j2_cdata_store: skipped (no native target)")
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

local function fresh (ct)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = i - 100 end
  return a
end

local function fill_const (a, n)
  for i = 0, n - 1 do a[i] = 7 end
  return a[0], a[n // 2], a[n - 1]
end

local function fill_index (a, n)
  for i = 0, n - 1 do a[i] = i end
  return a[0], a[n // 2], a[n - 1]
end

local function fill_var (a, n, v)
  for i = 0, n - 1 do a[i] = v end
  return a[0], a[n // 2], a[n - 1]
end

local function fill_const_float (a, n)
  for i = 0, n - 1 do a[i] = 1.25 end
  return a[0], a[n // 2], a[n - 1]
end

local function fill_step (a, n)
  for i = 0, n - 1, 3 do a[i] = 9 end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function fill_down (a, n)
  for i = n - 1, 0, -2 do a[i] = i end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function fill_ptr (a, n)
  local p = ffi.cast("int32_t *", a)
  for i = 0, n - 1 do p[i] = 5 end
  return a[0], a[n // 2], a[n - 1]
end

local function fill_oob (a, n)
  for i = 0, n - 1 do a[i] = 1 end
  return a[0]
end

local function run (name, ctor, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, ctor(), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, ctor(), N, table.unpack(args)) }
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

local function i32 () return fresh("int32_t") end
local function i8 () return fresh("int8_t") end
local function u8 () return fresh("uint8_t") end
local function f64 () return fresh("double") end

H.check(run("const int", i32, fill_const) > 0, "constant int fill did not use tint")
H.check(run("index int", i32, fill_index) > 0, "index fill did not use tint")
H.check(run("variable int", i32, fill_var, 300) > 0, "variable fill did not use tint")
H.check(run("variable wrap i8", i8, fill_var, 300) > 0, "int8 fill did not use tint")
H.check(run("variable wrap u8", u8, fill_var, -1) > 0, "uint8 fill did not use tint")
H.check(run("variable float->int", i32, fill_var, 3.9) > 0,
        "float-to-int fill did not use tint")
H.check(run("variable huge->int", i32, fill_var, 1e19) > 0,
        "out-of-range fill did not use tint")
H.check(run("const double", f64, fill_const_float) > 0,
        "constant double fill did not use tint")
H.check(run("index double", f64, fill_index) > 0,
        "index double fill did not use tint")
H.check(run("strided", i32, fill_step) > 0, "strided fill did not use tint")
H.check(run("descending", i32, fill_down) > 0,
        "descending fill did not use tint")

-- A pointer has no length, so the intrinsic must decline and the
-- interpreter remains authoritative.
H.eq(run("pointer fallback", i32, fill_ptr), 0,
     "a pointer store must not use the bounded helper")
run("out of bounds", i32, fill_oob, N + 1)

-- The loop control is finished like FORLOOP; re-entering the function must
-- see the interpreter's post-loop state.
do
  local function twice (a, n, v)
    for i = 0, n - 1 do a[i] = v end
    local first = a[n - 1]
    for i = 0, n - 1 do a[i] = first end
    return a[0], a[n - 1]
  end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local w1, w2 = twice(i32(), N, 4)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local g1, g2 = twice(i32(), N, 4)
  H.eq(g1, w1, "two-loop fill first result")
  H.eq(g2, w2, "two-loop fill second result")
end

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/130_j2_cdata_store ok")
