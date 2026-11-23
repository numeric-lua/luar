-- ffi/151_j2_bool_fill.lua -- J2 stores into bounded _Bool cdata arrays.
--
-- A bool element read yields a Lua boolean, so reductions and in-place
-- arithmetic stay on the interpreter.  Destinations are store-only: the
-- helper applies the C _Bool normalization (every non-zero scalar is 1),
-- never exposes a loaded element, and also covers copy/scalar/pair stores
-- whose bytecode result is a number.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/151_j2_bool_fill: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/151_j2_bool_fill: skipped (no native target)")
  return
end

local N = 257

local function make_bool ()
  local a = ffi.new("bool[?]", N)
  for i = 0, N - 1 do a[i] = false end
  return a
end

local function bool_bytes (a)
  return ffi.string(ffi.cast("uint8_t *", a), N)
end

local function run (name, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  local a = make_bool()
  local want = { pcall(f, a, N, table.unpack(args)) }
  local want_bytes = bool_bytes(a)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local b = make_bool()
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
  H.eq(bool_bytes(b), want_bytes, name .. ": bytes")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
end

-- Every non-zero scalar, including negative integers and fractional floats,
-- normalizes to 1; zero and -0.0 stay false.
run("bool constant 2", function (a, n)
  for i = 0, n - 1 do a[i] = 2 end
  return a[0], a[n - 1]
end)

run("bool constant 0", function (a, n)
  for i = 0, n - 1 do a[i] = 0 end
  return a[0], a[n - 1]
end)

run("bool constant -1", function (a, n)
  for i = 0, n - 1 do a[i] = -1 end
  return a[0], a[n - 1]
end)

run("bool fractional", function (a, n, v)
  for i = 0, n - 1 do a[i] = v end
  return a[0], a[n - 1]
end, 0.5)

run("bool loop key", function (a, n)
  for i = 0, n - 1 do a[i] = i end
  return a[0], a[1], a[n - 1]
end)

run("bool value slot", function (a, n, v)
  for i = 0, n - 1 do a[i] = v end
  return a[0], a[n - 1]
end, -0.0)

-- Copy/scalar/pair stores also normalize the numeric result.
run("bool copy", function (d, n)
  local src = ffi.new("int32_t[?]", n)
  for i = 0, n - 1 do src[i] = (i % 3) - 1 end
  for i = 0, n - 1 do d[i] = src[i] end
  return d[0], d[1], d[n - 1]
end)

run("bool scalar store", function (d, n, k)
  local src = ffi.new("int32_t[?]", n)
  for i = 0, n - 1 do src[i] = i % 5 end
  for i = 0, n - 1 do d[i] = src[i] * k end
  return d[0], d[1], d[n - 1]
end, 0)

-- A bool array captured as an upvalue is also a pure fill destination.
do
  local a
  local function set (v) a = v end
  local function f (n, v)
    for i = 0, n - 1 do a[i] = v end
  end
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  set(make_bool())
  f(N, 3)
  local want = bool_bytes(a)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(make_bool())
  local before = jit.stats().tint_hits
  f(N, 3)
  H.eq(bool_bytes(a), want, "bool upvalue fill bytes")
  H.check(jit.stats().tint_hits - before > 0, "bool upvalue fill intrinsic")
  H.eq(a[N - 1], true, "bool upvalue fill value")
end

-- Reading still yields booleans and a reduction stays interpreted.
do
  local a = ffi.new("bool[?]", N)
  for i = 0, N - 1 do a[i] = (i % 2 == 0) end
  H.eq(type(a[0]), "boolean")
  local s = 0
  for i = 0, N - 1 do if a[i] then s = s + 1 end end
  H.eq(s, (N + 1) // 2)
end

jit.flush()
print("ffi/151_j2_bool_fill ok")
