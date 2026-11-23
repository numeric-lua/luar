-- ffi/136_j2_cdata_pair_div.lua -- default J2 two-source cdata division.
--
-- `a[i] = b[i] / c[i]` joins the add/subtract/multiply pair stores.  The
-- helper follows the interpreter's Lua-number division (always float),
-- then converts the result to the destination element type.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/136_j2_cdata_pair_div: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/136_j2_cdata_pair_div: skipped (no native target)")
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

local function div (d, b, c, n)
  for i = 0, n - 1 do d[i] = b[i] / c[i] end
  return d[0], d[n // 2], d[n - 1]
end

local function step_div (d, b, c, n)
  for i = 0, n - 1, 3 do d[i] = b[i] / c[i] end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function down_div (d, b, c, n)
  for i = n - 1, 0, -2 do d[i] = b[i] / c[i] end
  return d[0], d[1], d[n - 2], d[n - 1]
end

local function ptr_div (d, b, c, n)
  local p = ffi.cast("int32_t *", b)
  for i = 0, n - 1 do d[i] = p[i] / c[i] end
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

local ints = function (i) return i * 37 - 5000 end
local small = function (i) return (i % 7) + 1 end
local doubles = function (i) return i * 0.25 - 3.5 end
local divisors = function (i) return (i % 5) + 0.5 end
local zero = function () return 0 end
local one = function () return 1 end

H.check(run("int divide", "int32_t", "int32_t", "int32_t",
            ints, small, div) > 0, "integer pair divide did not use tint")
H.check(run("double divide", "double", "double", "double",
            doubles, divisors, div) > 0,
        "double pair divide did not use tint")
H.check(run("int -> double", "double", "int32_t", "int32_t",
            ints, small, div) > 0,
        "int-to-double pair divide did not use tint")
H.check(run("strided", "double", "double", "double",
            doubles, divisors, step_div) > 0,
        "strided pair divide did not use tint")
H.check(run("descending", "double", "double", "double",
            doubles, divisors, down_div) > 0,
        "descending pair divide did not use tint")
H.check(run("divide by zero", "double", "double", "double",
            doubles, zero, div) > 0,
        "zero divisor did not use tint")

-- The earlier pair forms keep working after adding division.
H.check(run("add", "int32_t", "int32_t", "int32_t", ints, small,
            function (d, b, c, n)
              for i = 0, n - 1 do d[i] = b[i] + c[i] end
              return d[0], d[n - 1]
            end) > 0, "pair add regressed")

H.eq(run("pointer fallback", "double", "double", "double",
         doubles, divisors, ptr_div), 0,
     "a pointer source must not use the bounded helper")
run("out of bounds", "double", "double", "double", doubles, one,
    div, N + 1)

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/136_j2_cdata_pair_div ok")
