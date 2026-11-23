-- ffi/154_j4_ptr_field_widths.lua -- J4 pointer-field elements honor their
-- declared width or stay on the interpreter.
--
-- The pointer-field helpers are typed as 4-byte integer and 8-byte
-- double; accepting char*/short*/float* fields used the wrong stride and
-- could read or overwrite neighbouring elements.  Those widths now decline
-- to the interpreter, while int* and double* keep tracing.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/154_j4_ptr_field_widths: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/154_j4_ptr_field_widths: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct J4PtrWidths {
  char *cp;
  short *sp;
  int *ip;
  float *fp;
  double *dp;
} J4PtrWidths;
]]

local N = 64
local ac = ffi.new("char[?]", N)
local as = ffi.new("short[?]", N)
local ai = ffi.new("int[?]", N)
local af = ffi.new("float[?]", N)
local ad = ffi.new("double[?]", N)
for i = 0, N - 1 do
  ac[i] = i + 1
  as[i] = i * 2
  ai[i] = i * 3
  af[i] = i + 0.25
  ad[i] = i + 0.5
end
local p = ffi.new("J4PtrWidths")
p.cp, p.sp, p.ip, p.fp, p.dp = ac, as, ai, af, ad

local function sum_char (p, n)
  local x = 0
  for i = 0, n - 1 do x = x + p.cp[i] end
  return x
end
local function sum_short (p, n)
  local x = 0
  for i = 0, n - 1 do x = x + p.sp[i] end
  return x
end
local function sum_int (p, n)
  local x = 0
  for i = 0, n - 1 do x = x + p.ip[i] end
  return x
end
local function sum_float (p, n)
  local x = 0.0
  for i = 0, n - 1 do x = x + p.fp[i] end
  return x
end
local function sum_double (p, n)
  local x = 0.0
  for i = 0, n - 1 do x = x + p.dp[i] end
  return x
end

local function check_sum (name, f, want_native)
  jit.flush()
  jit.opt.start("hotloop=1", "ir=1")
  jit.off()
  local want = f(p, N)
  jit.flush()
  jit.opt.start("hotloop=1", "ir=1")
  jit.on()
  local before = jit.stats().ir_executed
  local got = f(p, N)
  H.eq(got, want, name .. ": sum")
  if want_native then
    H.check(jit.stats().ir_executed > before, name .. ": not traced")
  end
end

check_sum("char", sum_char, false)
check_sum("short", sum_short, false)
check_sum("int", sum_int, true)
check_sum("float", sum_float, false)
check_sum("double", sum_double, true)

-- Stores use the same width classification.  A rejected width must not
-- write past its elements; byte comparison catches a wrong stride.
local function fill_short (p, n)
  for i = 0, n - 1 do p.sp[i] = 100 + i end
end
local function fill_float (p, n)
  for i = 0, n - 1 do p.fp[i] = i + 0.5 end
end

local function check_store (name, f)
  local s = ffi.new("short[?]", N)
  local t = ffi.new("float[?]", N)
  local q = ffi.new("J4PtrWidths")
  q.sp, q.fp = s, t
  jit.flush()
  jit.opt.start("hotloop=1", "ir=1")
  jit.off()
  f(q, N)
  local want_s = ffi.string(ffi.cast("uint8_t *", s), N * 2)
  local want_t = ffi.string(ffi.cast("uint8_t *", t), N * 4)

  local s2 = ffi.new("short[?]", N)
  local t2 = ffi.new("float[?]", N)
  local q2 = ffi.new("J4PtrWidths")
  q2.sp, q2.fp = s2, t2
  jit.flush()
  jit.opt.start("hotloop=1", "ir=1")
  jit.on()
  f(q2, N)
  local got_s = ffi.string(ffi.cast("uint8_t *", s2), N * 2)
  local got_t = ffi.string(ffi.cast("uint8_t *", t2), N * 4)
  H.eq(got_s, want_s, name .. ": short bytes")
  H.eq(got_t, want_t, name .. ": float bytes")
end

check_store("pointer field widths", function (q, n)
  fill_short(q, n)
  fill_float(q, n)
end)

jit.flush()
print("ffi/154_j4_ptr_field_widths ok")
