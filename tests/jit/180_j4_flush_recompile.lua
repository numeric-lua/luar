-- jit/180_j4_flush_recompile.lua -- jit.flush() must let a J4 root trace
-- recompile.  The old install check only tested luar_ir_code == NULL, so a
-- stale pointer from the unmapped trace blocked every later recording.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/180_j4_flush_recompile: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()

local function f (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end

H.eq(f(1000), 500500)
H.check(jit.stats().ir_compiled >= 1, "IR root was not compiled")

jit.flush()
H.eq(jit.stats().ir_compiled, 0, "jit.flush left ir_compiled set")
H.eq(f(1000), 500500)
H.check(jit.stats().ir_compiled >= 1,
        "IR root did not recompile after jit.flush")
H.eq(jit.stats().ir_executed, 1)

H.eq(f(2000), 2001000)
H.check(jit.stats().ir_executed >= 2, "recompiled trace did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/180_j4_flush_recompile ok")
