-- ffi/163_bitfield_read_types.lua -- a bitfield read has the same
-- Lua-visible type as an ordinary field of its declared base type.
--
-- Bitfield loads always pushed a Lua integer.  A _Bool bitfield therefore
-- returned 1/0 instead of true/false, and an 8-byte bitfield returned a Lua
-- integer, so an unsigned 64-bit all-ones field became -1 instead of a
-- uint64 cdata.  That is observable through type(), tostring() and
-- tonumber().
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

ffi.cdef[[
struct BFType {
  unsigned long long u : 64;
  long long s : 64;
  unsigned long long u40 : 40;
  unsigned int u32 : 32;
  int s5 : 5;
  _Bool b : 1;
};
union BFUnion {
  unsigned long long u : 64;
  long long s : 64;
  _Bool b : 1;
};
]]

local v = ffi.new("struct BFType")
v.u = ffi.cast("uint64_t", -1)
v.s = -1
v.u40 = ffi.cast("uint64_t", 0xFFFFFFFFFF)
v.u32 = 0xFFFFFFFF
v.s5 = -3
v.b = 1

H.check(type(v.u) == "cdata", "unsigned 64-bit bitfield must be cdata")
H.check(type(v.s) == "cdata", "signed 64-bit bitfield must be cdata")
H.check(type(v.u40) == "cdata", "40-bit bitfield keeps its 8-byte type")
H.check(type(v.u32) == "number", "32-bit unsigned bitfield is a Lua number")
H.check(type(v.s5) == "number", "narrow signed bitfield is a Lua number")
H.check(type(v.b) == "boolean", "_Bool bitfield is a boolean")

H.check(ffi.istype("unsigned long long", v.u),
        "unsigned 64-bit bitfield lost its declared type")
H.check(ffi.istype("long long", v.s),
        "signed 64-bit bitfield lost its declared type")
H.eq(v.u, ffi.cast("uint64_t", -1), "unsigned 64-bit value")
H.eq(v.s, -1, "signed 64-bit value")
H.eq(tonumber(v.u40), 0xFFFFFFFFFF, "40-bit value")
H.eq(v.u32, 0xFFFFFFFF, "32-bit unsigned value")
H.eq(v.s5, -3, "narrow signed value")
H.eq(v.b, true, "_Bool true")

-- The value must survive a round-trip through the setter and raw storage.
H.eq(ffi.string(ffi.cast("char *", v), ffi.sizeof(v)):byte(1), 0xFF)
v.u = ffi.cast("uint64_t", 0x8000000000000000)
H.eq(v.u, ffi.cast("uint64_t", 0x8000000000000000),
     "high-bit unsigned value after store")
H.eq(tostring(v.u):find("ULL", 1, true) ~= nil, true,
     "unsigned cdata formatting regressed")

v.b = 0
H.eq(v.b, false, "_Bool false")
v.b = 2
H.eq(v.b, true, "_Bool store normalises to true")

local u = ffi.new("union BFUnion")
u.u = ffi.cast("uint64_t", -1)
H.check(type(u.u) == "cdata", "union 64-bit bitfield must be cdata")
H.eq(u.u, ffi.cast("uint64_t", -1), "union unsigned 64-bit value")
u.u = ffi.cast("uint64_t", 0)
H.eq(u.b, false, "union _Bool bitfield clear")
u.b = 1
H.eq(u.b, true, "union _Bool bitfield set")
H.eq(u.u, ffi.cast("uint64_t", 1), "union store writes the low bit")

print("ffi/163_bitfield_read_types ok")
