-- jit/50_ir_mixed.lua -- interior side exits with table reads and leaf calls.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/50_ir_mixed: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 10001 do t[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function dbl (x) return x * 2 end
local function f (t, f, n)
  local s = 0
  for i = 1, n do
    local x = i + 1
    s = s + t[x] + f(i)
  end
  return s
end

H.eq(f(t, dbl, 10000), 150025000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "mixed root was not compiled")

local h = table.new(0, 10001)
for i = 1, 10001 do h[i] = i end
H.eq(f(h, dbl, 10000), 150025000)
st = jit.stats()
H.check(st.ir_side_traces >= 1, "mixed side was not recorded")
H.check(st.ir_side_compiled >= 1, "mixed side was not compiled")
H.check(st.ir_side_executed >= 1, "mixed side was not executed")
H.check(st.ir_side_start > st.ir_root_start,
        "mixed side exit did not start inside the loop body")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/50_ir_mixed ok")
