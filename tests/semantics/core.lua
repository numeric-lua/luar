-- semantics/core.lua -- the Lua 5.5 core must behave exactly like upstream.

local H = dofile("tests/helpers.lua")

-- integers and floats are distinct
H.eq(math.type(1), "integer")
H.eq(math.type(1.0), "float")
H.eq(7 // 2, 3)
H.eq(-7 // 2, -4)
H.eq(7 / 2, 3.5)
H.eq(7 % 3, 1)
H.eq(2^10, 1024.0)
H.eq(math.maxinteger + 1, math.mininteger)
H.eq(math.mininteger // -1, math.mininteger)
H.eq(1 << 62, 4611686018427387904)
H.eq(~0, -1)

-- string/number coercion follows 5.4+ rules
H.eq("10" + 1, 11)
H.eq(math.type("10" + 1), "integer")
H.check(not pcall(function() return "x" + 1 end))

-- integer division by zero is an error, float division is not
H.check(not pcall(function() return 1 // 0 end))
H.eq(1 / 0, math.huge)

-- varargs and multiple returns
local function count(...) return select("#", ...) end
H.eq(count(), 0)
H.eq(count(nil, nil), 2)
local function pack2(...) return {...}, select("#", ...) end
local t, n = pack2(1, nil, 3)
H.eq(n, 3)
H.eq(t[1], 1)
H.eq(t[3], 3)

-- goto and labels
local acc = 0
for i = 1, 10 do
  if i % 2 == 0 then goto continue end
  acc = acc + i
  ::continue::
end
H.eq(acc, 25)

-- closures and upvalues
local function counter()
  local n = 0
  return function() n = n + 1; return n end
end
local c = counter()
H.eq(c(), 1)
H.eq(c(), 2)

-- metamethods
local mt = {
  __index = function(_, k) return "default:" .. k end,
  __len = function() return 42 end,
  __call = function(self, x) return x * 2 end,
  __concat = function(a, b) return "cat" end,
}
local obj = setmetatable({}, mt)
H.eq(obj.missing, "default:missing")
H.eq(#obj, 42)
H.eq(obj(21), 42)
H.eq(obj .. "x", "cat")

-- to-be-closed variables
local log = {}
do
  local guard <close> = setmetatable({}, {
    __close = function() log[#log + 1] = "closed" end,
  })
  log[#log + 1] = "body"
end
H.eq(table.concat(log, ","), "body,closed")

-- coroutines
local co = coroutine.create(function(a)
  local b = coroutine.yield(a + 1)
  return b * 2
end)
local ok, v = coroutine.resume(co, 1)
H.check(ok)
H.eq(v, 2)
ok, v = coroutine.resume(co, 10)
H.check(ok)
H.eq(v, 20)
H.eq(coroutine.status(co), "dead")
H.eq(coroutine.close(co), true)

-- table.create (new in 5.5)
local pre = table.create(16, 8)
H.eq(#pre, 0)
pre[1] = "a"
H.eq(#pre, 1)

-- strings, packing, utf8
H.eq(string.pack("i4", 42), string.char(42, 0, 0, 0))
local i4 = string.unpack("i4", string.pack("i4", -7))
H.eq(i4, -7)
H.eq(utf8.char(0x1F600):len(), 4)
H.eq(utf8.len("aé😀"), 3)
H.eq(string.format("%q", "a\nb"), '"a\\\nb"')

-- os/io basics
H.check(type(os.time()) == "number")
H.check(io.open("tests/helpers.lua") ~= nil)

print("semantics/core ok")
