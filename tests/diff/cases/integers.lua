-- diff/integers.lua -- integer/float semantics must match Lua 5.5 exactly.
local out = {}
local function add (fmt, ...) out[#out + 1] = string.format(fmt, ...) end

add("types %s %s %s", math.type(1), math.type(1.0), math.type("2"))
add("div %s %s %s", 7 // 2, -7 // 2, 7 / 2)
add("mod %d %d", 7 % 3, -7 % 3)
add("wrap %d %d", math.maxinteger + 1, math.mininteger // -1)
add("shift %d %d %d", 1 << 62, -1 >> 63, ~0)
add("floatint %s %s", 1 == 1.0, math.type(1 + 0.0))
add("tostr %s %s %s", tostring(1), tostring(1.0), tostring(-0.0))
add("fmt %.17g", 0.1)
add("tonum %d %s", "10" + 1, tostring(tonumber("0x10")))
add("hex %d %d", 0xff, 0x7fffffffffffffff)
add("idiv0 %s", tostring(select(2, pcall(function() return 1 // 0 end))))
add("pow %s", tostring(2^10))
add("maxint %s", tostring(math.maxinteger))
add("minint %s", tostring(math.mininteger))
print(table.concat(out, "\n"))
