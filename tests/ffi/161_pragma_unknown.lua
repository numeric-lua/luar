-- ffi/161_pragma_unknown.lua -- benign pragmas are ignored; pack still works.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

-- Real headers carry diagnostic/optimisation pragmas.  They must not make
-- cdef fail, and must not change layout.
ffi.cdef[[
#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpadded"
#pragma clang diagnostic push
#pragma message("luar: pragma test")
#pragma unknown vendor extension
#pragma
typedef struct LuarPragmaPlain { char c; int i; } LuarPragmaPlain;
]]
H.eq(ffi.sizeof("LuarPragmaPlain"), 8)
H.eq(ffi.offsetof("LuarPragmaPlain", "i"), 4)

-- A pragma whose name merely starts with "pack" is not #pragma pack.
ffi.cdef[[
#pragma pack_vendor_extension
typedef struct LuarPragmaPackWord { char c; int i; } LuarPragmaPackWord;
]]
H.eq(ffi.sizeof("LuarPragmaPackWord"), 8)

-- #pragma pack still drives layout exactly as before.
ffi.cdef[[
#pragma pack(push, 1)
typedef struct LuarPragmaPacked { char c; int i; } LuarPragmaPacked;
#pragma pack(pop)
typedef struct LuarPragmaNatural { char c; int i; } LuarPragmaNatural;
]]
H.eq(ffi.sizeof("LuarPragmaPacked"), 5)
H.eq(ffi.offsetof("LuarPragmaPacked", "i"), 1)
H.eq(ffi.sizeof("LuarPragmaNatural"), 8)
H.eq(ffi.offsetof("LuarPragmaNatural", "i"), 4)

-- Unknown pragmas inside an inactive group are skipped without validation.
ffi.cdef[[
#if 0
#pragma pack(3)
#pragma whatever
#pragma GCC diagnostic this-is-not-checked
#endif
typedef struct LuarPragmaInactive { int x; } LuarPragmaInactive;
]]
H.eq(ffi.sizeof("LuarPragmaInactive"), 4)

-- Malformed pack pragmas are still errors.
H.raises(function () ffi.cdef("#pragma pack(3)\n") end, "bad #pragma pack")
H.raises(function () ffi.cdef("#pragma pack(pop)\n") end,
         "without push")
H.raises(function () ffi.cdef("#pragma pack(1\n") end, "unterminated")

print("ffi/161_pragma_unknown ok")
