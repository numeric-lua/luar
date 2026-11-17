-- diff/strings.lua -- strings, patterns, packing, utf8.
local out = {}
local function add (fmt, ...) out[#out + 1] = string.format(fmt, ...) end

add("len %d", #"héllo")
add("sub %s", ("hello"):sub(2, 4))
add("rep %s", ("ab"):rep(3, "-"))
add("find %s %s", ("abc123"):find("%d+"))
add("gsub %s %d", ("a1b2"):gsub("%d", "X"))
add("gmatch %s", table.concat((function ()
  local t = {}
  for w in ("a,b,c"):gmatch("[^,]+") do t[#t + 1] = w end
  return t
end)(), "|"))
add("format %q", string.format("%q", "a\nb\0c"))
add("pack %s", (string.pack("i4i8d", 42, -7, 0.5):gsub(".", function (ch)
  return string.format("%02x", ch:byte())
end)))
add("unpack %d %d %g", string.unpack("i4i8d", string.pack("i4i8d", 42, -7, 0.5)))
add("utf8 %d %s", utf8.len("aé😀"), utf8.char(0x1F600))
add("codes %s", (function ()
  local t = {}
  for _, cp in utf8.codes("aé") do t[#t + 1] = cp end
  return table.concat(t, ",")
end)())
add("byte %d %d", ("A"):byte(), ("A"):byte(1))
add("tostring %s", tostring(12) .. tostring(1.5))
print(table.concat(out, "\n"))
