-- ffi/109_pointer_compat.lua -- pointer arithmetic matches LuaJIT.
--
-- 'p - q' requires compatible base types (same-size integer types are
-- compatible; different struct tags, struct vs union, and different
-- element widths are not).  A floating Lua offset truncates towards zero;
-- out-of-range, NaN and infinite offsets behave as zero.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef[[
typedef struct PtrA { int x; } PtrA;
typedef struct PtrB { int x; } PtrB;
typedef union PtrU { int x; } PtrU;
typedef enum PtrE { PtrE1 } PtrE;
typedef int (*PtrFn)(int);
]]

local function diff (a, b) return a - b end
local function addr (p) return tonumber(ffi.cast("intptr_t", p)) end

-- Integer element types with the same size are compatible regardless of
-- signedness; typedefs and enums follow their underlying integer type.
local ip = ffi.cast("int *", 4096)
local up = ffi.cast("unsigned int *", 4088)
H.eq(diff(ip, up), 2)
H.eq(diff(ffi.cast("char *", 4096), ffi.cast("signed char *", 4088)), 8)
H.eq(diff(ffi.cast("PtrE *", 4096), ffi.cast("int *", 4088)), 2)
H.eq(diff(ffi.cast("intptr_t *", 4096), ffi.cast("long *", 4088)), 1)

-- Different element widths or aggregate identities are rejected.
H.raises(function () return diff(ip, ffi.cast("char *", 4088)) end,
         "pointer")
H.raises(function () return diff(ffi.cast("float *", 4096), ip) end,
         "pointer")
H.raises(function () return diff(ffi.cast("PtrA *", 4096),
                                 ffi.cast("PtrB *", 4088)) end, "pointer")
H.raises(function () return diff(ffi.cast("PtrA *", 4096),
                                 ffi.cast("PtrU *", 4088)) end, "pointer")
H.raises(function () return diff(ffi.cast("PtrFn", 4096),
                                 ffi.cast("PtrFn", 4088)) end, "pointer")
H.raises(function () return diff(ip, ffi.cast("PtrFn", 4088)) end,
         "pointer")

-- Array pointer base types are compared recursively, including length.
local a4 = ffi.cast("int (*)[4]", 4096)
H.eq(diff(a4, ffi.cast("unsigned int (*)[4]", 4064)), 2)
H.raises(function () return diff(a4, ffi.cast("int (*)[8]", 4064)) end,
         "pointer")

-- Floating offsets truncate towards zero, as in LuaJIT.
H.eq(addr(ip + 1.5), 4100)
H.eq(addr(ip - 1.5), 4092)
H.eq(addr(ip + 0.9), 4096)
H.eq(addr(ip - 0.9), 4096)
H.eq(addr(ip + ffi.new("double", 1.5)), 4100)

-- NaN, infinities and values outside int64 behave like a zero offset
-- instead of an undefined C conversion.
local nan = 0.0 / 0.0
H.eq(addr(ip + math.huge), 4096)
H.eq(addr(ip - math.huge), 4096)
H.eq(addr(ip + nan), 4096)
H.eq(addr(ip + 1e30), 4096)

print("ffi/109_pointer_compat ok")
