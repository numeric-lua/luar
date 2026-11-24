-- jit/198_j4_arith_meta.lua -- J4 integer arithmetic must not run on a
-- known table/string/userdata operand.
--
-- The integer analyzer did not track table-valued field reads, and it
-- admitted string/userdata guards as arithmetic operands.  A raw integer
-- ADD was then emitted, so Lua's __add handler was bypassed and a table or
-- userdata address was interpreted as an integer.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/198_j4_arith_meta: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1")
jit.on()

-- A table-valued field read used in arithmetic must call __add.
local add_calls, sub_calls = 0, 0
local obj = {
  x = setmetatable({}, {
    __add = function () add_calls = add_calls + 1; return 1 end,
    __sub = function () sub_calls = sub_calls + 1; return 1 end,
  }),
}
local function add_field (o, n)
  local s = 0
  for i = 1, n do s = s + o.x end
  return s
end
H.eq(add_field(obj, 1000), 1, "table field __add result")
H.eq(add_calls, 1000, "table field __add calls")
H.eq(math.type(add_field(obj, 1000)), "integer", "table field __add type")

-- The same guard must cover subtraction.
local function sub_field (o, n)
  local s = 0
  for i = 1, n do s = s - o.x end
  return s
end
H.eq(sub_field(obj, 1000), 1, "table field __sub result")
H.eq(sub_calls, 1000, "table field __sub calls")

-- String values with an arithmetic metatable must invoke it.
local str = "hello"
local oldmt = debug.getmetatable(str)
debug.setmetatable(str, { __add = function () return 1 end })
local function add_str (x, n)
  local s = 0
  for i = 1, n do s = s + x end
  return s
end
H.eq(add_str(str, 1000), 1, "string __add")
debug.setmetatable(str, oldmt)

-- Userdata values with an arithmetic metatable must invoke it too.
local u = io.stdout
local umt = debug.getmetatable(u)
debug.setmetatable(u, { __add = function () return 1 end })
local function add_userdata (x, n)
  local s = 0
  for i = 1, n do s = s + x end
  return s
end
H.eq(add_userdata(u, 1000), 1, "userdata __add")
debug.setmetatable(u, umt)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("jit/198_j4_arith_meta ok")
