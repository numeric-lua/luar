-- jit/162_while_float_limit.lua -- a fast while loop must not compare a
-- float limit as an integer.
--
-- The `while i < limit do acc = acc + i; i = i + 1 end` peephole emitted an
-- integer compare/load for the limit but the later branch-guard pass used
-- the live float tag.  A subnormal double whose bit pattern is 100 made the
-- native loop run 200 iterations instead of 101; a finite 100.0 could hang.
-- Float limits now fall back to the generic branch emitter, which performs
-- the interpreter's mixed int/float comparison.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/162_while_float_limit: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=0")

local function count_up (n)
  local i, s = -100, 0
  while i < n do
    s = s + i
    i = i + 1
  end
  return s, i
end

local function check (name, n)
  jit.flush()
  jit.off()
  local want_s, want_i = count_up(n)
  jit.flush()
  jit.on()
  local got_s, got_i = count_up(n)
  H.check(same(want_s, got_s) and same(want_i, got_i),
          name .. ": result differs from the interpreter")
  return jit.stats()
end

-- A finite subnormal whose 64-bit pattern is a small integer.  The old
-- fast path compared that pattern as an integer and produced -100, 100.
local tiny = string.unpack("d", string.pack("I8", 100))
local st = check("subnormal float limit", tiny)
H.check(st.compiled >= 1, "float-limit loop did not compile")
H.check(st.sig_hits > 0, "float-limit loop did not run natively")

-- Ordinary fractional and NaN limits.
check("fractional float limit", 100.5)
check("NaN float limit", 0 / 0)
check("negative tiny float limit", -1e-300)

-- An integer limit still uses the native fast path.
local function int_limit (n)
  local i, s = 0, 0
  while i < n do
    s = s + i
    i = i + 1
  end
  return s, i
end
jit.flush()
jit.off()
local want = int_limit(200)
jit.flush()
jit.on()
local before = jit.stats().sig_hits
H.eq(int_limit(200), want, "integer-limit result")
H.check(jit.stats().compiled >= 1, "integer-limit loop did not compile")
H.check(jit.stats().sig_hits > before, "integer-limit loop did not run natively")

jit.flush()
jit.opt.start("hotloop=56")
print("jit/162_while_float_limit ok")
