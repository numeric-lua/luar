-- ffi/139_format_q_cdata.lua -- string.format("%q", cdata).
--
-- Luar's FFI wrapper already routed numeric cdata through Lua 5.5's
-- formatter, but %q only reached values that ffi_is_numeric accepted.
-- Pointers, arrays, aggregates and ctype objects therefore fell through to
-- stock %q and raised "value has no literal form".  LuaJIT formats them
-- through tostring and quotes the result; do the same without changing
-- numeric conversions.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/139_format_q_cdata: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef [[ typedef struct { int x; } FmtStruct; ]]

local function quoted (s, what)
  H.eq(type(s), "string", what .. " is not a string")
  H.eq(s:sub(1, 1), '"', what .. " is missing the opening quote")
  H.eq(s:sub(-1), '"', what .. " is missing the closing quote")
  return s
end

local p = ffi.cast("int *", 0)
quoted(string.format("%q", p), "NULL pointer")
H.check(string.format("%q", p):find("NULL", 1, true) ~= nil,
        "NULL pointer lost its tostring form")
quoted(string.format("%q", ffi.new("int[3]")), "array")
quoted(string.format("%q", ffi.new("FmtStruct")), "struct")
quoted(string.format("%q", ffi.new("double", 1.5)), "double cdata")
quoted(string.format("%q", ffi.typeof("int")), "ctype object")

H.eq(string.format("%q", ffi.new("int64_t", 42)), '"42LL"')
H.eq(string.format("%q", ffi.new("uint64_t", 42)), '"42ULL"')

-- %s keeps using the same tostring representation for every cdata kind.
H.eq(string.format("%s", p), tostring(p))
H.check(string.format("%s", ffi.new("FmtStruct")):find("cdata<", 1, true) ~= nil,
        "%s lost the struct representation")
H.check(string.format("%s", ffi.new("int[3]")):find("cdata<", 1, true) ~= nil,
        "%s lost the cdata representation")

-- Numeric conversions are unchanged.
H.eq(string.format("%d", ffi.new("int32_t", -3)), "-3")
H.eq(string.format("%x", ffi.new("uint64_t", 255)), "ff")
H.eq(string.format("%5.2f", 1.5), " 1.50")

-- A metatype __tostring participates in %q exactly as tostring does.
do
  local mt = { __tostring = function (c) return "FMT(" .. c.x .. ")" end }
  ffi.metatype("FmtStruct", mt)
  local s = ffi.new("FmtStruct", 7)
  H.eq(string.format("%s", s), "FMT(7)")
  H.eq(string.format("%q", s), '"FMT(7)"')
end

-- Non-cdata arguments are untouched.
H.eq(string.format("%q", "a\nb"), '"a\\\nb"')
H.eq(string.format("%q", 12), "12")
H.eq(string.format("%q", nil), "nil")
H.eq(string.format("%q", true), "true")

print("ffi/139_format_q_cdata ok")
