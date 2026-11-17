-- diff/errors.lua -- error propagation and pcall/xpcall shapes.
local out = {}
local function add (fmt, ...) out[#out + 1] = string.format(fmt, ...) end

local ok, e = pcall(function () error(nil) end)
add("nilerr %s %s", tostring(ok), tostring(e))
ok, e = pcall(function () error({ code = 7 }) end)
add("objerr %s %s", tostring(ok), tostring(type(e)) .. ":" .. tostring(e.code))
ok, e = pcall(function () local x = nil; return x.y end)
add("runtime %s %s", tostring(ok), tostring((tostring(e):gsub("^.-:%d+: ", ""))))
ok, e = xpcall(function () error("inner") end, function (m) return "handled:" .. m end)
add("xpcall %s %s", tostring(ok), tostring(e))
add("assert %d", select("#", assert(1, 2, 3)))

local callable = function () return "ok" end
for _ = 1, 10 do callable = setmetatable({}, { __call = callable }) end
add("callchain %s", tostring(callable()))
for _ = 1, 10 do callable = setmetatable({}, { __call = callable }) end
ok, e = pcall(callable)
add("callchain-limit %s %s", tostring(ok), tostring(e))
print(table.concat(out, "\n"))
