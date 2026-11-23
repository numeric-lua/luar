-- ffi/137_j2_cdata_i64_fill.lua -- default J2 fills of 8-byte integer arrays.
--
-- `a[i] = value|i|constant` can now target int64_t/uint64_t arrays.  These
-- are pure store contexts: no element is loaded into a Lua slot, so the
-- helper can apply the FFI C-store conversion without creating cdata
-- temporaries.  Copy/update/pair forms intentionally stay interpreted
-- because their visible temporaries are cdata int64 values.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/137_j2_cdata_i64_fill: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/137_j2_cdata_i64_fill: skipped (no native target)")
  return
end

local N = 128

local function make (ct)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = 0 end
  return a
end

local function fill_var (a, n, v)
  for i = 0, n - 1 do a[i] = v end
  return a[0], a[1], a[n // 2], a[n - 1]
end

local function fill_index (a, n)
  for i = 0, n - 1 do a[i] = i end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function fill_const (a, n)
  for i = 0, n - 1 do a[i] = -3 end
  return a[0], a[n // 2], a[n - 1]
end

local function fill_step (a, n)
  for i = 0, n - 1, 3 do a[i] = 7 end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function fill_down (a, n)
  for i = n - 1, 0, -2 do a[i] = i end
  return a[0], a[1], a[n - 2], a[n - 1]
end

local function run (name, ct, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(ct), N, table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(ct), N, table.unpack(args)) }
  local hits = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.eq(tostring(got[i]), tostring(want[i]),
           name .. ": result " .. i)
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  return hits
end

H.check(run("int64 fill value", "int64_t", fill_var, 42) > 0,
        "int64 value fill did not use tint")
H.check(run("int64 fill negative", "int64_t", fill_var, -1) > 0,
        "int64 negative fill did not use tint")
H.check(run("int64 fill float", "int64_t", fill_var, 3.9) > 0,
        "int64 float fill did not use tint")
H.check(run("int64 fill huge", "int64_t", fill_var, 1e19) > 0,
        "out-of-range int64 fill did not use tint")
H.check(run("int64 fill index", "int64_t", fill_index) > 0,
        "int64 index fill did not use tint")
H.check(run("int64 fill constant", "int64_t", fill_const) > 0,
        "int64 constant fill did not use tint")
H.check(run("int64 strided", "int64_t", fill_step) > 0,
        "int64 strided fill did not use tint")
H.check(run("int64 descending", "int64_t", fill_down) > 0,
        "int64 descending fill did not use tint")
H.check(run("uint64 fill negative", "uint64_t", fill_var, -1) > 0,
        "uint64 wrap fill did not use tint")
H.check(run("uint64 fill huge", "uint64_t", fill_var, 1e19) > 0,
        "uint64 out-of-range fill did not use tint")
H.check(run("uint64 fill index", "uint64_t", fill_index) > 0,
        "uint64 index fill did not use tint")

-- 8-byte element reads remain cdata, so copy and arithmetic updates stay
-- on the interpreter to preserve their visible temporaries.
local function copy64 (d, s, n)
  for i = 0, n - 1 do d[i] = s[i] end
  return d[0], d[n - 1]
end
local function update64 (a, n, k)
  for i = 0, n - 1 do a[i] = a[i] + k end
  return a[0], a[n - 1]
end
do
  local d = make("int64_t")
  local s = make("int64_t")
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local a, b = copy64(d, s, N)
  H.eq(a, 0, "int64 copy destination")
  H.eq(b, 0, "int64 copy destination tail")
  H.eq(jit.stats().tint_hits, before,
       "int64 copy must not use the Lua-number store helper")
  before = jit.stats().tint_hits
  a, b = update64(d, N, 5)
  H.eq(a, 5, "int64 update destination")
  H.eq(jit.stats().tint_hits, before,
       "int64 update must not use the Lua-number store helper")
end

-- Out-of-bounds runs fall back and keep raising from the interpreter.
run("out of bounds", "int64_t", fill_var, 1, N + 1)

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/137_j2_cdata_i64_fill ok")
