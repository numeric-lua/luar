-- ffi/106_pointer_arith.lua -- pointer arithmetic needs a complete object type.
--
-- C (and LuaJIT) reject arithmetic on 'void *' and on pointers to
-- incomplete structs/unions because there is no element size.  Luar used
-- to fall back to one byte, which hid type errors behind byte arithmetic.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef[[
struct Incomplete;
typedef struct Incomplete *IncompleteP;
]]

local vp = ffi.cast("void *", 4096)
H.raises(function () return vp + 1 end, "pointer arithmetic")
H.raises(function () return vp - 1 end, "pointer arithmetic")
H.raises(function () return vp - ffi.cast("void *", 4096) end,
         "pointer arithmetic")

local ip = ffi.cast("IncompleteP", 4096)
H.raises(function () return ip + 1 end, "pointer arithmetic")
H.raises(function () return 1 + ip end, "pointer arithmetic")
H.raises(function () return ip - 1 end, "pointer arithmetic")
H.raises(function () return ip - ffi.cast("IncompleteP", 4096) end,
         "pointer arithmetic")

-- Complete object types keep working, including the non-commutative '-'.
local p = ffi.cast("int *", 4096)
H.eq(tonumber(ffi.cast("uintptr_t", p + 2)), 4104)
H.eq(tonumber(ffi.cast("uintptr_t", 2 + p)), 4104)
H.eq(tonumber(ffi.cast("uintptr_t", p - 2)), 4088)
H.eq(tonumber((p + 3) - p), 3)

print("ffi/106_pointer_arith ok")
