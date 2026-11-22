-- jit/127_exact_cmp.lua -- J2 mixed integer/float comparisons are exact.
--
-- Converting a 64-bit integer to a double rounds values outside
-- [-2^53, 2^53], so a naive ucomisd comparison disagreed with the
-- interpreter.  The native loop must leave such a comparison to the
-- interpreter (and must not re-enter itself at every backedge).
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/127_exact_cmp: skipped (no native target)")
  return
end

local ops = { "==", "<", "<=", ">", ">=" }

local function run (op, src_expr, n)
  local cond = src_expr:gsub("%%op%%", op)
  local src = [[
    return function (n)
      local s = 0
      for i = 9007199254740991, 9007199254740991 + n do
        if ]] .. cond .. [[ then s = s + 1 end
      end
      return s
    end
  ]]
  local ref = assert(load(src, "=exactref"))()
  jit.flush()
  jit.off()
  local want = ref(n)
  local f = assert(load(src, "=exactjit"))()
  jit.flush()
  jit.on()
  local first = f(n)
  local second = f(n)
  local st = jit.stats()
  jit.flush()
  jit.off()
  local was = f(n)
  H.eq(first, want, op .. " " .. src_expr .. ": first call")
  H.eq(second, want, op .. " " .. src_expr .. ": native call")
  H.eq(was, want, op .. " " .. src_expr .. ": interpreter")
  H.check(st.compiled >= 1, op .. " " .. src_expr .. ": loop did not compile")
end

for _, op in ipairs(ops) do
  -- integer on the left, float constant on the right
  run(op, "i %op% 9007199254740992.0", 100)
  -- float constant on the left, integer on the right
  run(op, "9007199254740992.0 %op% i", 100)
end

-- Both operands are live registers: the integer is exact and the float is
-- a value that rounds several nearby integers to the same double.
local function vs_float (n)
  local f = 9007199254740992.0
  local s = 0
  for i = 9007199254740991, 9007199254740991 + n do
    if i == f then s = s + 1 end
    if i < f then s = s + 10 end
  end
  return s
end
jit.flush()
jit.off()
local want = vs_float(100)
jit.flush()
jit.on()
H.eq(vs_float(100), want)
H.eq(vs_float(100), want)

-- Small mixed comparisons stay on the native path and remain correct.
local small_src = [[
  return function (n)
    local s = 0
    for i = 1, n do
      if i * 0.5 < 1000 then s = s + i end
    end
    return s
  end
]]
local small_ref = assert(load(small_src, "=smallref"))()
jit.flush()
jit.off()
local small_want = small_ref(5000)
local small_f = assert(load(small_src, "=smalljit"))()
jit.flush()
jit.on()
H.eq(small_f(5000), small_want)
H.eq(small_f(5000), small_want)
H.check(jit.stats().compiled >= 1, "small mixed comparison did not compile")

jit.flush()
print("jit/127_exact_cmp ok")
