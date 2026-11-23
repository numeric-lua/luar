-- jit/163_ir_mixed_float.lua -- J4 floatify must not leave an integer
-- arithmetic op on a double operand.
--
-- The float pass used to convert only the chain rooted at the first float
-- slot it saw.  An independent `t = t * 1` or `u = s * -1` stayed an
-- integer MULK while its operand was a double, so the emitter read the
-- double payload as an integer.  A trace with any remaining integer op on
-- a float slot is now rejected; the interpreter produces the exact result.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/163_ir_mixed_float: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function pack (...)
  return {n = select("#", ...), ...}
end

local function same (want, got)
  for i = 1, want.n do
    if math.type(want[i]) ~= math.type(got[i]) then return false end
    if math.type(want[i]) == "float" then
      if want[i] ~= want[i] and got[i] ~= got[i] then
        -- no NaN in this test
      elseif bits(want[i]) ~= bits(got[i]) then
        return false
      end
    elseif want[i] ~= got[i] then
      return false
    end
  end
  return true
end

jit.opt.start("ir=1", "hotloop=1", "table=0", "tint=0")

local sources = {
  [[
    return function (n)
      local s, t, u = 2.5, 0.5, 0.5
      for i = n, 1, -1 do
        t = t * 1
        t = t * 0.5
        u = s * -1
      end
      return s, t, u
    end
  ]],
  [[
    return function (n)
      local s, t, u = -1, 0.0, 0
      for i = 1, n do
        s = t * 0.5
        t = s + u
        u = t * 2
        t = s + (((2 * -1) * 1.5) * 1)
        t = t / 3
      end
      return s, t, u
    end
  ]],
}

for case, src in ipairs(sources) do
  local f = assert(load(src, "=irmixed" .. case))()
  jit.flush()
  jit.off()
  local want = pack(f(128))
  jit.flush()
  jit.on()
  local got = pack(f(128))
  H.eq(got.n, want.n, "case " .. case .. ": result count")
  H.check(same(want, got),
          "case " .. case .. ": mixed float IR changed the result")
end

-- A supported all-float table trace still compiles and executes.
local t = {}
for i = 1, 100 do t[i] = i + 0.5 end
local function sub (tab, n)
  local s = 0
  for i = 1, n do s = s - tab[i] end
  return s
end
jit.flush()
jit.on()
H.eq(sub(t, 100), -5100.0)
H.check(jit.stats().ir_executed >= 1,
        "supported float table trace stopped executing")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
print("jit/163_ir_mixed_float ok")
