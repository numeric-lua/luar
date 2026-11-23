-- ffi/147_j2_cdata_upval_fill.lua -- J2 fills a cdata array held in a
-- closure upvalue.
--
-- `GETUPVAL a; SETTABLE a, i, v` is a pure fill, so the helper can write the
-- array without ever loading an element into a Lua slot.  This includes
-- 8-byte integer destinations.  Rebinding the upvalue after compilation is
-- observed on the next entry.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/147_j2_cdata_upval_fill: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/147_j2_cdata_upval_fill: skipped (no native target)")
  return
end

local N = 257

local function make_fill ()
  local a
  local function set (v) a = v end
  local function f (n, mode, value)
    if mode == "value" then
      for i = 0, n - 1 do a[i] = value end
    elseif mode == "index" then
      for i = 0, n - 1 do a[i] = i end
    elseif mode == "strided" then
      for i = 0, n - 1, 3 do a[i] = value end
    elseif mode == "descending" then
      for i = n - 1, 0, -2 do a[i] = i end
    end
  end
  return f, set
end

local function make (ct, fill)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = fill(i) end
  return a
end

local function bytes (a, ct)
  return ffi.string(ffi.cast("uint8_t *", a), N * ffi.sizeof(ct))
end

local function run (name, ct, mode, value)
  local f, set = make_fill()
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  local a = make(ct, function () return 11 end)
  set(a)
  f(N, mode, value)
  local want = bytes(a, ct)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local b = make(ct, function () return 11 end)
  set(b)
  local before = jit.stats().tint_hits
  f(N, mode, value)
  local hits = jit.stats().tint_hits - before
  H.eq(bytes(b, ct), want, name .. ": array bytes")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
end

run("int32 upvalue fill value", "int32_t", "value", 42)
run("int32 upvalue fill negative", "int32_t", "value", -7)
run("uint16 upvalue fill float", "uint16_t", "value", 3.9)
run("int64 upvalue fill value", "int64_t", "value", 1e19)
run("uint64 upvalue fill negative", "uint64_t", "value", -1)
run("int32 upvalue fill index", "int32_t", "index")
run("int16 upvalue strided", "int16_t", "strided", 7)
run("double upvalue descending", "double", "descending")

-- Rebinding after the loop compiled must target the new array.
do
  local f, set = make_fill()
  local a = make("int32_t", function () return 0 end)
  local b = make("int32_t", function () return 0 end)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  set(a)
  f(N, "value", 5)
  local a_bytes = bytes(a, "int32_t")
  set(b)
  f(N, "value", 9)
  H.eq(bytes(a, "int32_t"), a_bytes,
       "rebound fill left the old array alone")
  local want = make("int32_t", function () return 0 end)
  for i = 0, N - 1 do want[i] = 9 end
  H.eq(bytes(b, "int32_t"), bytes(want, "int32_t"),
       "rebound fill wrote the new array")
end
