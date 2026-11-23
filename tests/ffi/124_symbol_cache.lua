-- ffi/124_symbol_cache.lua -- repeated function-symbol lookups are cached
-- per namespace, matching LuaJIT's cdata identity.
--
-- clib_index used to run dlsym and allocate a new function cdata on every
-- access.  Variables and constants must stay snapshots, so only CT_FUNC
-- symbols are cached in the CLibrary userdata.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/124_symbol_cache: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
int getpid(void);
int abs(int);
]]

local a = ffi.C.getpid
local b = ffi.C.getpid
H.check(rawequal(a, b), "LuaJIT returns the same function cdata object")
H.eq(a, b, "cached cdata compares equal")
H.eq(type(a), "cdata")
H.eq(a(), b(), "cached function is callable")
H.eq(ffi.C.abs(-7), 7, "a second symbol works")

-- The cache keeps the library alive and survives collection.
a, b = nil, nil
collectgarbage()
collectgarbage()
local c = ffi.C.getpid
H.check(rawequal(c, ffi.C.getpid), "cache survives a GC cycle")
H.eq(c(), ffi.C.getpid(), "cached function still calls")

-- A cdef re-declaration does not change the cached object's callability.
ffi.cdef[[int getpid(void);]]
H.eq(ffi.C.getpid(), c(), "redeclaration keeps the same symbol")

-- Separate namespaces cache separately; their function cdata compares by
-- address but is not the same object.
local okc, c_lib = pcall(ffi.load, "c")
if okc then
  local from_c = c_lib.getpid
  H.check(not rawequal(from_c, c), "different namespaces keep distinct cdata")
  H.check(from_c == c, "same C address compares equal across namespaces")
  H.eq(from_c(), c(), "both namespaces call the same function")
end

print("ffi/124_symbol_cache ok")
