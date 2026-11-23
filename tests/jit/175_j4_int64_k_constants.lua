-- jit/175_j4_int64_k_constants.lua -- J4 integer K-ops must keep all 64
-- bits of a constant.
--
-- The IR emitter used `add/sub/imul rax, imm` for ADDK/SUBK/MULK.  x86 only
-- encodes a sign-extended 32-bit immediate in those forms, so 2^32 became 0,
-- 2^31 became -2^31 and maxint became -1.  The J2 emitter already used the
-- scratch-register helpers; J4 now does too.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/175_j4_int64_k_constants: skipped (no native target)")
  return
end

-- Set the threshold before these Protos are created.
jit.opt.start("ir=1", "table=0", "tint=1", "hotloop=1")

local function add32 (n)
  local x = 3
  for i = 1, n do x = x + 4294967296 end
  return x
end

local function sub32 (n)
  local x = 3
  for i = 1, n do x = x - 4294967296 end
  return x
end

local function mul32p1 (n)
  local x = 3
  for i = 1, n do x = x * 4294967297 end
  return x
end

local function addmax (n)
  local x = 3
  for i = 1, n do x = x + 0x7fffffffffffffff end
  return x
end

local function mulmax (n)
  local x = 3
  for i = 1, n do x = x * 0x7fffffffffffffff end
  return x
end

local function run (name, f, n)
  jit.flush()
  jit.off()
  local want = f(n)
  jit.flush()
  jit.opt.start("ir=1", "table=0", "tint=1", "hotloop=1")
  jit.on()
  for _ = 1, 20 do f(n) end
  local got = f(n)
  H.eq(got, want, name .. ": J4 result differs from interpreter")
  H.check(jit.stats().ir_compiled >= 1 and jit.stats().ir_executed > 0,
          name .. ": loop did not run through the J4 IR")
end

run("add 2^32", add32, 100)
run("sub 2^32", sub32, 100)
run("mul 2^32+1", mul32p1, 20)
run("add maxint", addmax, 5)
run("mul maxint", mulmax, 5)

jit.flush()
jit.opt.start("ir=0", "table=0", "tint=1", "call=0", "alloc=0", "hotloop=56", "hotexit=10")
print("jit/175_j4_int64_k_constants ok")
