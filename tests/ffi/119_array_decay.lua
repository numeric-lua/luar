-- ffi/119_array_decay.lua -- array cdata decays to a pointer for arithmetic
-- and address comparison, as in LuaJIT.
--
-- `ffi.new("int[4]") + 1` yields `int *`, array - array yields the element
-- distance, and an array compares equal to a pointer to its first element.
-- Ordering is only defined for compatible base types; `int[2] < char *`
-- and function-pointer ordering stay errors.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef[[
typedef struct { int x; double y; } DecayS;
size_t strlen(const char *);
]]

local a = ffi.new("int[4]", { 10, 20, 30, 40 })
local b = ffi.new("int[4]", { 1, 2, 3, 4 })
local p = ffi.cast("int *", a)
local s = ffi.new("DecayS[2]")
s[0].x = 5
s[1].x = 6

-- Array + integer is pointer arithmetic; the result is a pointer.
local p1 = a + 1
H.eq(p1[0], 20, "a + 1 did not point at the second element")
H.eq((1 + a)[0], 20, "1 + a did not commute")
H.eq((a - 1)[1], 10, "a - 1 moved the wrong way")
H.check(ffi.istype("int *", p1), "array arithmetic did not produce int *")
H.check(ffi.istype("int *", a - 1), "array subtraction did not produce int *")
H.eq(ffi.sizeof(a + 1), ffi.sizeof("int *"),
     "pointer result has the wrong size")

-- Fractional offsets truncate towards zero, exactly like pointer offsets.
H.eq((a + 1.9)[0], 20, "fractional array offset did not truncate")
H.eq((a - 1.9)[1], 10, "negative fractional offset did not truncate")

-- Pointer/array differences return a Lua integer, not cdata.
local diff = (a + 2) - a
H.eq(math.type(diff), "integer", "array distance is not a Lua integer")
H.eq(diff, 2, "array distance")
H.eq(a - p, 0, "array - pointer distance")
H.eq(p - a, 0, "pointer - array distance")
H.eq((a + 3) - a, 3, "array - array distance")

-- Struct arrays decay with the struct element size.
local sp = s + 1
H.eq(sp.x, 6, "struct array decay read the wrong element")
H.eq((sp - 1)[0].x, 5, "struct pointer moved by the wrong element size")
H.eq((s + 1)[0].x, 6, "struct array indexing after decay")

-- Equality compares addresses for array/pointer-like operands.  Two arrays
-- remain distinct unless they are the same object, and an array never
-- compares equal to nil or a number.
H.check(a == p, "array == pointer to first element")
H.check(p == a, "pointer == array to first element")
H.check(a ~= b, "distinct arrays compared equal")
H.check(a ~= ffi.new("int[4]", { 10, 20, 30, 40 }),
        "distinct equal-valued arrays compared equal")
H.check(a ~= nil, "array compared equal to nil")
H.check(a ~= 0, "array compared equal to a number")
H.check(s == ffi.cast("DecayS *", s), "struct array did not decay for equality")
H.check(ffi.C.strlen(ffi.new("char[4]", "abc")) == 3,
        "array argument did not decay to char *")

-- Ordering is address ordering when the base types are compatible.
H.check(a <= a, "array <= itself")
H.check(a < a + 1, "array < next element")
H.check((a + 1) > a, "next element > array")
H.check(a <= p, "array <= pointer to first element")
H.check(p <= a, "pointer <= array to first element")
H.check(s < s + 1, "struct array ordering")

-- Incompatible base types and function pointers are rejected, as in LuaJIT.
H.raises(function() return a < ffi.cast("char *", a) end,
         "attempt to compare", "array < char pointer")
H.raises(function() return a < ffi.cast("void *", a) end,
         "attempt to compare", "array < void pointer")
H.raises(function() return ffi.cast("int *", a) < ffi.cast("char *", a) end,
         nil, "the existing pointer/pointer order path is unchanged")
H.raises(function() return a < 0 end, "attempt to compare",
         "array < number")

local f = ffi.C.strlen
H.check(f ~= a, "function pointer compared equal to an array")
H.check(not (f < f) and f <= f, "function pointer self-ordering")
H.raises(function() return f + 1 end, nil,
         "function pointer arithmetic still fails")
H.raises(function() return f < a end, "attempt to compare",
         "function pointer ordering against an array still fails")
H.raises(function() return f < ffi.cast("void *", f) end,
         "attempt to compare", "function pointer ordering against void *")

-- Plain pointer arithmetic and comparisons keep working.
H.eq((p + 2)[0], 30, "pointer arithmetic regressed")
H.eq(p[1], 20, "pointer indexing regressed")
H.check(p == ffi.cast("int *", a), "pointer equality regressed")

print("ffi/119_array_decay ok")
