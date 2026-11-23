-- ffi/141_j2_cdata_pair_divmod.lua -- default J2 two-source cdata
-- floor-division and modulo stores.
--
-- `a[i] = b[i] // c[i]` and `a[i] = b[i] % c[i]` over bounded cdata arrays
-- run through a C helper.  The helper follows Lua's floor-division and
-- modulo rules (including negative operands), rejects an integer zero
-- divisor before any store so the interpreter raises the exact error, and
-- keeps the source temporaries observable after the loop.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/141_j2_cdata_pair_divmod: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/141_j2_cdata_pair_divmod: skipped (no native target)")
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

local function idiv (d, b, c, n)
  for i = 0, n - 1 do d[i] = b[i] // c[i] end
  return d[0], d[n // 2], d[n - 1]
end

local function mod (d, b, c, n)
  for i = 0, n - 1 do d[i] = b[i] % c[i] end
  return d[0], d[n // 2], d[n - 1]
end

local function idiv_step (d, b, c, n)
  for i = 0, n - 1, 3 do d[i] = b[i] // c[i] end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function mod_down (d, b, c, n)
  for i = n - 1, 0, -2 do d[i] = b[i] % c[i] end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function ptr_div (d, b, c, n)
  local p = ffi.cast("int32_t *", b)
  for i = 0, n - 1 do d[i] = p[i] // c[i] end
  return d[0], d[n - 1]
end

local function run (name, dct, bct, cct, binit, cinit, f, ...)
  local args = { ... }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = { pcall(f, make(dct, function () return 0 end),
                       make(bct, binit), make(cct, cinit), N,
                       table.unpack(args)) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, make(dct, function () return 0 end),
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

local nums = function (i) return i * 37 - 5000 end
local divs = function (i) return (i % 9) + 1 end
local negs = function (i) return ((i % 11) + 1) * (i % 2 == 0 and 1 or -1) end
local doubles = function (i) return i * 0.25 - 3.5 end
local divisors = function (i) return (i % 5) + 0.5 end
local zero_i = function () return 0 end
local zero_f = function () return 0.0 end

H.check(run("int idiv", "int32_t", "int32_t", "int32_t",
            nums, divs, idiv) > 0, "integer idiv did not use tint")
H.check(run("int mod", "int32_t", "int32_t", "int32_t",
            nums, divs, mod) > 0, "integer mod did not use tint")
H.check(run("int negative idiv", "int32_t", "int32_t", "int32_t",
            nums, negs, idiv) > 0, "negative idiv did not use tint")
H.check(run("int negative mod", "int32_t", "int32_t", "int32_t",
            nums, negs, mod) > 0, "negative mod did not use tint")
H.check(run("double idiv", "double", "double", "double",
            doubles, divisors, idiv) > 0, "double idiv did not use tint")
H.check(run("double mod", "double", "double", "double",
            doubles, divisors, mod) > 0, "double mod did not use tint")
H.check(run("int -> double", "double", "int32_t", "int32_t",
            nums, divs, idiv) > 0, "int-to-double idiv did not use tint")
H.check(run("strided idiv", "int32_t", "int32_t", "int32_t",
            nums, divs, idiv_step) > 0, "strided idiv did not use tint")
H.check(run("descending mod", "int32_t", "int32_t", "int32_t",
            nums, divs, mod_down) > 0, "descending mod did not use tint")
H.check(run("small destination", "int8_t", "int32_t", "int32_t",
            nums, divs, mod) > 0, "int8 idiv destination did not use tint")

-- Float zero is not an error for // or %, so the helper handles it.
H.check(run("double idiv by zero", "double", "double", "double",
            doubles, zero_f, idiv) > 0, "float idiv by zero did not use tint")
H.check(run("double mod by zero", "double", "double", "double",
            doubles, zero_f, mod) > 0, "float mod by zero did not use tint")

-- Integer zero raises; the helper must decline before storing anything.
H.eq(run("int idiv by zero", "int32_t", "int32_t", "int32_t",
         nums, zero_i, idiv), 0,
     "integer idiv by zero must stay on the interpreter")
H.eq(run("int mod by zero", "int32_t", "int32_t", "int32_t",
         nums, zero_i, mod), 0,
     "integer mod by zero must stay on the interpreter")

-- A pointer source has no length and stays interpreted.
H.eq(run("pointer fallback", "int32_t", "int32_t", "int32_t",
         nums, divs, ptr_div), 0,
     "a pointer source must not use the bounded helper")

-- 8-byte element reads yield cdata, not Lua numbers.
H.eq(run("int64 source fallback", "int32_t", "int64_t", "int32_t",
         function (i) return i + 1 end, divs, idiv), 0,
     "an int64 source must stay interpreted")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/141_j2_cdata_pair_divmod ok")
