-- tests/ffi/01_basics.lua -- module surface, CType objects and ABI info.

local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

H.eq(type(ffi), "table")
H.eq(ffi.os, "Linux")
H.eq(ffi.arch, "x64")

-- ffi.abi: values relevant to a 64-bit little-endian Linux/x64 build.
H.eq(ffi.abi("64bit"), true)
H.eq(ffi.abi("32bit"), false)
H.eq(ffi.abi("le"), true)
H.eq(ffi.abi("be"), false)
H.eq(ffi.abi("fpu"), true)
H.eq(ffi.abi("hardfp"), true)
H.eq(ffi.abi("softfp"), false)
H.eq(ffi.abi("win"), false)
H.eq(ffi.abi("eabi"), false)
H.eq(ffi.abi("gc64"), true)
H.eq(ffi.abi("no-such-abi"), false)

-- Primitive sizes/alignments for the x86-64 System V ABI.
H.eq(ffi.sizeof("void *"), 8)
H.eq(ffi.sizeof("char"), 1)
H.eq(ffi.sizeof("signed char"), 1)
H.eq(ffi.sizeof("unsigned char"), 1)
H.eq(ffi.sizeof("short"), 2)
H.eq(ffi.sizeof("unsigned short"), 2)
H.eq(ffi.sizeof("int"), 4)
H.eq(ffi.sizeof("unsigned int"), 4)
H.eq(ffi.sizeof("long"), 8)
H.eq(ffi.sizeof("unsigned long"), 8)
H.eq(ffi.sizeof("long long"), 8)
H.eq(ffi.sizeof("unsigned long long"), 8)
H.eq(ffi.sizeof("float"), 4)
H.eq(ffi.sizeof("double"), 8)
H.eq(ffi.sizeof("bool"), 1)
H.eq(ffi.alignof("char"), 1)
H.eq(ffi.alignof("short"), 2)
H.eq(ffi.alignof("int"), 4)
H.eq(ffi.alignof("long"), 8)
H.eq(ffi.alignof("double"), 8)
H.eq(ffi.alignof("void *"), 8)

H.eq(ffi.sizeof("int8_t"), 1)
H.eq(ffi.sizeof("uint8_t"), 1)
H.eq(ffi.sizeof("int16_t"), 2)
H.eq(ffi.sizeof("uint16_t"), 2)
H.eq(ffi.sizeof("int32_t"), 4)
H.eq(ffi.sizeof("uint32_t"), 4)
H.eq(ffi.sizeof("int64_t"), 8)
H.eq(ffi.sizeof("uint64_t"), 8)
H.eq(ffi.sizeof("intptr_t"), 8)
H.eq(ffi.sizeof("uintptr_t"), 8)
H.eq(ffi.sizeof("size_t"), 8)
H.eq(ffi.sizeof("ptrdiff_t"), 8)

H.eq(ffi.sizeof("int[10]"), 40)
H.eq(ffi.alignof("int[10]"), 4)
H.eq(ffi.sizeof("int *[3]"), 24)
H.eq(ffi.sizeof("int (*)[3]"), 8)
H.eq(ffi.sizeof("int (*)(int)"), 8)
H.eq(ffi.alignof("int (*)(int)"), 8)
H.eq(ffi.sizeof("struct { double d; }"), 8)
H.eq(ffi.alignof("struct { char c; double d; }"), 8)

-- cdef, typedefs, structs and enums.
H.eq(select("#", ffi.cdef [[
typedef struct Basic { char c; int i; } Basic;
enum BasicE { BE_A, BE_B = 4, BE_C };
enum { ANON_A = 7, ANON_B };
]]), 0)
H.eq(ffi.sizeof("Basic"), 8)
H.eq(ffi.sizeof("struct Basic"), 8)
H.eq(ffi.offsetof("Basic", "i"), 4)
H.eq(ffi.sizeof("enum BasicE"), 4)
H.eq(ffi.C.BE_A, 0)
H.eq(ffi.C.BE_B, 4)
H.eq(ffi.C.BE_C, 5)
H.eq(ffi.C.ANON_A, 7)
H.eq(ffi.C.ANON_B, 8)

-- CType objects are cdata and interned.
local ti = ffi.typeof("int")
H.eq(type(ti), "cdata")
H.eq(tostring(ti), "ctype<int>")
H.check(rawequal(ti, ffi.typeof("int")))
H.check(not rawequal(ti, ffi.typeof("unsigned int")))
H.eq(tostring(ffi.typeof(ffi.new("int"))), "ctype<int>")
local tb = ffi.typeof("Basic")
H.eq(type(tb), "cdata")
H.check(tostring(tb):find("^ctype<") ~= nil)
H.eq(tostring(ffi.typeof(tb)), tostring(tb))
H.eq(ffi.sizeof(tb), 8)
H.eq(ffi.alignof(tb), 4)

H.eq(ffi.istype("int", ffi.new("int")), true)
H.eq(ffi.istype("unsigned int", ffi.new("int")), false)
H.eq(ffi.istype("Basic", ffi.new("struct Basic")), true)
H.eq(ffi.istype("Basic", ffi.new("int")), false)
H.eq(ffi.istype("Basic", ffi.new("Basic *")), true)
H.eq(ffi.istype("Basic *", ffi.new("Basic *")), true)
H.eq(ffi.istype("int *", ffi.new("Basic *")), false)
H.eq(ffi.istype("int *", ffi.new("void *")), false)
H.eq(ffi.istype("int", ti), true)
H.eq(ffi.istype("int", ffi.new("int64_t")), false)

-- ffi.NULL is nil in LuaJIT; a cast NULL pointer is a cdata that compares
-- equal to nil.
H.eq(ffi.NULL, nil)
H.eq(type(ffi.NULL), "nil")
local np = ffi.cast("void *", 0)
H.eq(type(np), "cdata")
H.check(tostring(np):find("NULL", 1, true) ~= nil)
H.eq(np == nil, true)
H.eq(nil == np, true)
H.check(not (ffi.new("int", 0) == nil))
H.check(not (ffi.new("int64_t", 0) == nil))
H.check(not (ffi.new("double", 0) == nil))
H.check(not (ffi.new("int[1]", {0}) == nil))

-- ffi.errno reads and sets the C errno.
ffi.errno(0)
H.eq(ffi.errno(), 0)
H.eq(ffi.errno(7), 0)
H.eq(ffi.errno(), 7)
ffi.errno(0)

-- The Lua type() wrapper must only change cdata classification.
H.eq(type(1), "number")
H.eq(type("x"), "string")
H.eq(type({}), "table")
H.eq(type(print), "function")
H.eq(type(io.stdout), "userdata")
H.eq(type(np), "cdata")

print("ffi/01_basics ok")
