-- jit/126_const64.lua -- J2 keeps full 64-bit immediate operands.
--
-- x86-64 add/sub/imul accept only a sign-extended 32-bit immediate.  Large
-- Lua integer constants used to be encoded through that field, so the
-- compiled loop disagreed with the interpreter.  Each case calls the hot
-- function twice: the first call warms the loop, the second runs natively.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/126_const64: skipped (no native target)")
  return
end

local consts = {
  "2147483648",
  "-2147483649",
  "4294967296",
  "-4294967297",
  "9007199254740992",
  "9007199254740993",
  "9223372036854775807",
  "(-9223372036854775807 - 1)",
}

local function check (name, src, n, want_compiled)
  local f = assert(load(src, "=jit126"))
  jit.flush()
  jit.opt.start("ir=0")
  jit.on()
  local first = f()(n)
  local second = f()(n)
  local st = jit.stats()
  jit.flush()
  jit.off()
  local want = f()(n)
  H.eq(first, want, name .. ": first call")
  H.eq(second, want, name .. ": native call")
  if want_compiled ~= false then
    H.check(st.compiled >= 1, name .. ": loop did not compile")
  end
end

for _, k in ipairs(consts) do
  check("add " .. k,
        "return function (n) local s = 3; " ..
        "for i = 1, n do s = s + (" .. k .. ") end; return s end", 100)
  check("sub " .. k,
        "return function (n) local s = 3; " ..
        "for i = 1, n do s = s - (" .. k .. ") end; return s end", 100)
  check("mul " .. k,
        "return function (n) local s = 3; " ..
        "for i = 1, n do s = s * (" .. k .. ") end; return s end", 100)
end

-- The __index multiplier fast path has its own immediate multiply.
local meta_src = [[
  return function (n)
    local o = setmetatable({}, {
      __index = function (_, k) return k * 1099511627776 end,
    })
    local s = 0
    for i = 1, n do s = s + o[i] end
    return s
  end
]]
local meta_ref = assert(load(meta_src, "=jit126meta"))()
jit.flush()
jit.off()
local meta_want = meta_ref(100)
local meta_f = assert(load(meta_src, "=jit126meta"))()
jit.flush()
jit.on()
H.eq(meta_f(100), meta_want)
H.eq(meta_f(100), meta_want)
local st = jit.stats()
H.check(st.compiled >= 1, "metatable multiplier did not compile")

-- A float accumulator with a full-width integer multiplier also goes
-- through the register path.
local flt_src = [[
  return function (n)
    local x = 0.0
    for i = 1, n do x = x + i * 1099511627776 end
    return x
  end
]]
local flt_ref = assert(load(flt_src, "=jit126flt"))()
jit.flush()
jit.off()
local want = flt_ref(100)
local flt_f = assert(load(flt_src, "=jit126flt"))()
jit.flush()
jit.on()
local got = flt_f(100)
H.eq(got, want, "float accumulator with a 64-bit multiplier")

jit.flush()
print("jit/126_const64 ok")
