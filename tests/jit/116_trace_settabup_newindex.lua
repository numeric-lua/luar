-- jit/116_trace_settabup_newindex.lua -- upvalue writes must respect
-- __newindex; trace helpers reject metatable-protected tables.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/116_trace_settabup_newindex: skipped (no native target)")
  return
end

local hits = 0
local t = setmetatable({}, {
  __newindex = function (tab, k, v)
    hits = hits + 1
    rawset(tab, k, v)
  end,
})
local function f (n)
  for _ = 1, n do t.x = 2.5 end
  return t.x
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.near(f(1000), 2.5)
H.eq(hits, 1, "__newindex must run exactly once for the new key")
H.near(t.x, 2.5)
H.check(jit.stats().ir_compiled >= 1, "SETTABUP trace did not compile")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/116_trace_settabup_newindex ok")
