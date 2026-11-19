-- jit/54_ir_side_fallback.lua -- side failure must resume at the exact exit PC.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/54_ir_side_fallback: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local t = setmetatable({}, {
  __index = function () return 7 end,
})
local function f (t, n)
  local s = 0
  for i = 1, n do
    s = s + i       -- executed prefix before the table lookup
    s = s + t[i]    -- side fails here; interpreter must resume here
  end
  return s
end

H.eq(f(t, 100), 5050 + 700)
H.check(jit.stats().ir_side_traces >= 1, "fallback side was not recorded")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/54_ir_side_fallback ok")
