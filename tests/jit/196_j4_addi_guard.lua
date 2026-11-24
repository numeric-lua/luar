-- jit/196_j4_addi_guard.lua -- J4's ADDI/FADDI selection must be guarded
-- by the operand subtype.
--
-- The recorder chose FADDI for a float operand and ADDI for an integer one
-- but emitted no guard.  Re-entering the same trace with the other flavour
-- either returned a float for `2 + 1` or reinterpreted the double payload
-- as an integer.  Both flavours must now either match the recorded guard or
-- exit to the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/196_j4_addi_guard: skipped (no native target)")
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

local function run (name, f, x, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.off()
  local want = f(x, n)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
  jit.on()
  local got = f(x, n)
  H.check(same(got, want), name .. ": " .. tostring(got) ..
          " (" .. math.type(got) .. ") ~= " .. tostring(want) ..
          " (" .. math.type(want) .. ")")
end

local function addi (x, n)
  local s = 0
  for i = 1, n do s = x + 1 end
  return s
end

-- Same Proto, both entry flavours, in both recording orders.
run("int first", addi, 2, 1000)
run("float after int", addi, 2.5, 1000)
run("float first", addi, 1.5, 1000)
run("int after float", addi, 2, 1000)

-- A loop-carried slot that changes flavour between iterations must exit.
local function carried (n)
  local x = 1
  local s = 0
  for i = 1, n do
    s = s + x
    x = x + 1.5
  end
  return s, x
end
jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.off()
local ws, wx = carried(30)
jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()
local gs, gx = carried(30)
H.check(same(gs, ws), "carried accumulator")
H.check(same(gx, wx), "carried slot type")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/196_j4_addi_guard ok")
