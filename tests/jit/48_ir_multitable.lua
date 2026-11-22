-- jit/48_ir_multitable.lua -- interior side exits from multi-table IR bodies.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/48_ir_multitable: skipped (no native target)")
  return
end

local a, b = {}, {}
jit.off()
for i = 1, 10000 do a[i] = i; b[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function f (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] * 2 end
  return s
end

H.eq(f(a, b, 10000), 150015000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "multi-table root was not compiled")

local h = table.new(0, 10000)
for i = 1, 10000 do h[i] = i end
H.eq(f(a, h, 10000), 150015000)
st = jit.stats()
H.check(st.ir_side_traces >= 1, "multi-table side was not recorded")
H.check(st.ir_side_compiled >= 1, "multi-table side was not compiled")
H.check(st.ir_side_executed >= 1, "multi-table side was not executed")
H.check(st.ir_side_start > st.ir_side_parent,
        "side exit did not start inside a multi-table body")
H.check(st.ir_side_depth >= 1, "side depth was not recorded")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/48_ir_multitable ok")
