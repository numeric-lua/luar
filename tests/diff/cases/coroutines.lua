-- diff/coroutines.lua -- resume/yield/close/status.
local out = {}
local function add (fmt, ...) out[#out + 1] = string.format(fmt, ...) end

local co = coroutine.create(function (a)
  local b = coroutine.yield(a + 1)
  return b * 2
end)
add("status %s", coroutine.status(co))
local ok, v = coroutine.resume(co, 1)
add("resume1 %s %d %s", tostring(ok), v, coroutine.status(co))
ok, v = coroutine.resume(co, 10)
add("resume2 %s %d %s", tostring(ok), v, coroutine.status(co))

local gen = coroutine.wrap(function ()
  for i = 1, 3 do coroutine.yield(i) end
end)
add("wrap %d %d %d", gen(), gen(), gen())

add("isyieldable %s %s", tostring(coroutine.isyieldable()),
    tostring(coroutine.wrap(function () return coroutine.isyieldable() end)()))

local closeco = coroutine.create(function () coroutine.yield() end)
coroutine.resume(closeco)
add("close %s %s", tostring(coroutine.close(closeco)), coroutine.status(closeco))
print(table.concat(out, "\n"))
