-- jit/45_ir_interior.lua -- interior side trace from a prefixed IR table body.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/45_ir_interior: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 20001 do t[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function read (t, n)
  local s = 0
  for i = 1, n do
    local x = i + 1
    s = s + t[x] * 2
  end
  return s
end

H.eq(read(t, 20000), 400060000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "prefixed IR root was not compiled")
H.check(st.ir_executed >= 1, "prefixed IR root was not executed")

local u = table.new(0, 20001)
for i = 1, 20001 do u[i] = i end
H.eq(read(u, 20000), 400060000)
st = jit.stats()
H.check(st.ir_side_traces >= 1, "interior side trace was not recorded")
H.check(st.ir_side_compiled >= 1, "interior side trace was not compiled")
H.check(st.ir_side_executed >= 1, "interior side trace was not executed")
H.check(st.ir_side_start >= 0, "interior side start PC missing")
H.check(st.ir_side_start > st.ir_side_parent,
        "side exit did not start inside the loop body")
H.check(st.ir_cycles == 0, "interior side exit was misclassified as a cycle")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/45_ir_interior ok")
