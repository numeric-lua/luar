-- ffi/174_nested_offsetof.lua -- dotted member designators in the
-- `offsetof` constant expression.
--
-- C's offsetof accepts a member-designator such as `i.b`, not just a
-- direct field name.  The cdef constant-expression parser now walks the
-- dotted path through nested structs/unions (including anonymous members)
-- and sums the offsets; the final member may not be a bitfield.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

ffi.cdef[[
typedef struct { int a; double b; } NInner;
typedef struct { char c; NInner i; int tail; } NOuter;
typedef struct { char c; union { int x; double y; }; } NAnon;
typedef struct { NInner inner[2]; int z; } NArr;
enum {
  OFF_IA = offsetof(NOuter, i.a),
  OFF_IB = offsetof(NOuter, i.b),
  OFF_TAIL = offsetof(NOuter, tail),
  OFF_Y = offsetof(NAnon, y),
  OFF_ARR = offsetof(NArr, inner[1].a)
};
_Static_assert(offsetof(NOuter, i.b) == 16, "nested offset");
_Static_assert(offsetof(NAnon, y) == 8, "anonymous union offset");
]]

H.eq(ffi.C.OFF_IA, 8, "nested int")
H.eq(ffi.C.OFF_IB, 16, "nested double")
H.eq(ffi.C.OFF_TAIL, 24, "direct trailing field")
H.eq(ffi.C.OFF_Y, 8, "anonymous union member")
H.eq(ffi.C.OFF_ARR, ffi.sizeof("NInner"), "array member path")

-- A dotted path may also size a declaration.
ffi.cdef[[
typedef struct { char pad[offsetof(NOuter, tail)]; } NPad;
]]
H.eq(ffi.sizeof("NPad"), 24, "offsetof in an array size")

-- Unknown path components and bitfield leaves are clear errors.
H.raises(function ()
  ffi.cdef[[ _Static_assert(offsetof(NOuter, i.missing) == 0, "bad"); ]]
end, "offsetof")
H.raises(function ()
  ffi.cdef[[ typedef struct { int a:3; } NB; _Static_assert(offsetof(NB, a) == 0, "bad"); ]]
end, "bitfield")

print("ffi/174_nested_offsetof ok")
