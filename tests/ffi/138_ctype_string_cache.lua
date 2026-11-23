-- ffi/138_ctype_string_cache.lua -- cached parsing of C type names.
--
-- ffi.new/ffi.cast/ffi.sizeof/ffi.typeof accept type strings and parse them
-- on every call unless the interned CType is cached.  The cache is keyed by
-- the exact spelling and only holds successful parses, so cdef can still
-- define a missing typedef or complete a forward struct afterwards.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/138_ctype_string_cache: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

-- Repeated lookups return the same interned ctype, with the same size.
local a = ffi.typeof("int[4]")
local b = ffi.typeof("int[4]")
H.check(a == b, "repeated ffi.typeof did not return the same ctype")
H.eq(ffi.sizeof("int[4]"), 16)
H.eq(ffi.sizeof(a), 16)

-- A cached VLA type still computes a per-call size.
local vla = ffi.typeof("int[?]")
H.eq(ffi.sizeof(vla, 3), 12)
H.eq(ffi.sizeof(vla, 9), 36)
H.eq(ffi.sizeof("int[?]", 5), 20)
local v = ffi.new("int[?]", 7)
H.eq(ffi.sizeof(v), 28)
H.check(ffi.typeof(v) == ffi.typeof("int[?]"),
        "ctype of a VLA cdata differs")

-- A failed parse is not cached: after cdef defines the name, it succeeds.
H.raises(function () return ffi.typeof("CachedAlias") end, "C type")
ffi.cdef [[ typedef int CachedAlias; ]]
H.eq(ffi.sizeof("CachedAlias"), 4)
H.eq(ffi.sizeof(ffi.new("CachedAlias", 3)), 4)

-- A successful parse of a forward struct is reused and updated in place
-- when cdef completes the definition.
ffi.cdef [[ struct CachedForward; ]]
local fwd = ffi.typeof("struct CachedForward")
H.raises(function () return ffi.sizeof(fwd) end, "unknown")
ffi.cdef [[ struct CachedForward { int x; double y; }; ]]
H.check(ffi.sizeof(fwd) > 8, "cached forward struct was not completed")
H.eq(ffi.sizeof("struct CachedForward"), ffi.sizeof(fwd))

-- The cache cap must not change behaviour for names beyond the cap.
for i = 1, 1100 do
  H.eq(ffi.sizeof("int[" .. i .. "]"), i * 4)
end
H.eq(ffi.sizeof("int[2048]"), 8192)

-- Pointer and function types use the same path.
local p = ffi.new("int *")
H.check(ffi.istype("int *", p), "pointer ctype cache changed istype")
H.eq(ffi.sizeof("int (*)(int)"), 8)
H.eq(ffi.alignof("double"), 8)
H.raises(function () return ffi.sizeof("NoSuchType") end, "C type")

print("ffi/138_ctype_string_cache ok")
