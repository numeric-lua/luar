-- diff/language.lua -- closures, goto, varargs, metamethods, <close>.
local out = {}
local function add (fmt, ...) out[#out + 1] = string.format(fmt, ...) end

local function counter ()
  local n = 0
  return function () n = n + 1; return n end
end
local c = counter()
add("closure %d %d", c(), c())

local acc = 0
for i = 1, 10 do
  if i % 2 == 0 then goto continue end
  acc = acc + i
  ::continue::
end
add("goto %d", acc)

local function va (...) return select("#", ...), ... end
local n, a, b, d = va(1, nil, 3)
add("varargs %d %s %s %s", n, tostring(a), tostring(b), tostring(d))
add("select %d %d", select("#", nil, nil), select(2, "a", "b", "c"))

local mt = {
  __index = function (_, k) return "i:" .. k end,
  __len = function () return 42 end,
  __call = function (_, x) return x * 2 end,
  __add = function (a, b) return "add" end,
}
local o = setmetatable({}, mt)
add("meta %s %d %d %s", o.x, #o, o(21), o + 1)

local log = {}
do
  local g <close> = setmetatable({}, { __close = function () log[#log + 1] = "closed" end })
  log[#log + 1] = "body"
end
add("close %s", table.concat(log, ","))

local t = table.create(4, 4)
t[1] = "a"
add("create %d %s", #t, t[1])

add("concat %s", table.concat({1, 2, 3}, "-"))
add("sort %s", table.concat((function ()
  local a = {5, 2, 9, 1}
  table.sort(a, function (x, y) return x > y end)
  return a
end)(), ","))
print(table.concat(out, "\n"))
