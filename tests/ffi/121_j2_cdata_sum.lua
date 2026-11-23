-- ffi/121_j2_cdata_sum.lua -- default J2 reduction over bounded cdata arrays.
--
-- `for i = 0, n - 1 do s = s + a[i] end` is the canonical numeric-FFI loop.
-- J4 could already trace it behind jit.opt.start("ir=1"); the J2 tier now
-- recognises the same shape and runs a bounds-checked C helper for the
-- remaining iterations.  The helper must preserve int wrap, unsigned
-- element widening, int->float promotion, the post-loop element temporary
-- and the loop control slots.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/121_j2_cdata_sum: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/121_j2_cdata_sum: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

local N = 257
local I = ffi.new("int[?]", N)
local U = ffi.new("uint32_t[?]", N)
local S8 = ffi.new("int8_t[?]", N)
local U8 = ffi.new("uint8_t[?]", N)
local F = ffi.new("float[?]", N)
local D = ffi.new("double[?]", N)
for i = 0, N - 1 do
  I[i] = i * 37 - 5000
  U[i] = 0x80000000 + i * 7
  S8[i] = i * 5 - 600
  U8[i] = i * 3
  F[i] = i * 0.25 - 3.5
  D[i] = i * 0.1 + 0.3
end

local function sum_i (a, n)
  local s, x = 0, 0
  for i = 0, n - 1 do x = a[i]; s = s + x end
  return s, x
end

local function sum_f (a, n)
  local s, x = 0.0, 0.0
  for i = 0, n - 1 do x = a[i]; s = s + x end
  return s, x
end

local function sum_if (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end

local function sum_fi (a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end

local function sum_step (a, n)
  local s, x = 0, 0
  for i = 0, n - 1, 3 do x = a[i]; s = s + x end
  return s, x
end

local function sum_down (a, n)
  local s = 0
  for i = n - 1, 0, -2 do s = s + a[i] end
  return s
end

local cases = {
  {"int32 temp", sum_i, I, N},
  {"uint32 temp", sum_i, U, N},
  {"int8 temp", sum_i, S8, N},
  {"uint8 temp", sum_i, U8, N},
  {"float64 temp", sum_f, D, N},
  {"float32 temp", sum_f, F, N},
  {"int32 acc float elems", sum_fi, F, N},
  {"float acc int elems", sum_fi, I, N},
  {"int->float promotion", sum_if, F, N},
  {"strided", sum_step, I, N},
  {"descending", sum_down, D, N},
  {"empty range", sum_i, I, 0},
}

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local want = {}
for c = 1, #cases do
  local name, f, a, n = table.unpack(cases[c])
  local r = { pcall(f, a, n) }
  want[c] = r
end

jit.flush()
jit.on()
local before = jit.stats().tint_hits
for c = 1, #cases do
  local name, f, a, n = table.unpack(cases[c])
  -- The first call makes the loop hot; the second exercises the installed
  -- intrinsic.  Some shapes legitimately stay interpreted (pointer/empty).
  local got = { pcall(f, a, n) }
  if n > 0 then
    got = { pcall(f, a, n) }
  end
  H.eq(got[1], want[c][1], name .. ": error status")
  if got[1] then
    H.eq(#got, #want[c], name .. ": result count")
    for k = 2, #want[c] do
      H.check(same(got[k], want[c][k]),
              name .. ": result " .. k .. " differs: " ..
              tostring(got[k]) .. " ~= " .. tostring(want[c][k]))
    end
  else
    H.check(tostring(got[2]):find("out of bounds", 1, true) ~= nil or
            tostring(got[2]) == tostring(want[c][2]),
            name .. ": error message differs")
  end
end
H.check(jit.stats().tint_hits > before,
        "the cdata reduction intrinsic did not run")

-- A pointer has no length to bound-check, so the intrinsic must not claim
-- it; the interpreter path remains authoritative and correct.
local p = ffi.cast("int *", I)
local function sum_ptr (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end
jit.off()
local wp = sum_ptr(p, N)
jit.on()
H.eq(sum_ptr(p, N), wp, "pointer fallback")

-- Out-of-bounds runs must still raise exactly like the interpreter.
local function sum_oob (a, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] end
  return s
end
jit.off()
local want_err = select(2, pcall(sum_oob, I, N + 1))
jit.on()
local got_err = select(2, pcall(sum_oob, I, N + 1))
H.eq(tostring(got_err), tostring(want_err), "out-of-bounds error")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/121_j2_cdata_sum ok")
