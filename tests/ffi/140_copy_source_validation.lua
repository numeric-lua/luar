-- ffi/140_copy_source_validation.lua -- ffi.copy validates its source even
-- when the requested length is zero.
--
-- A zero-length copy used to return before checking the source, so numbers,
-- tables and scalar cdata were silently accepted.  LuaJIT rejects those;
-- only a NULL pointer (nil) is a valid zero-length source.  Pointer/array
-- cdata sources are still validated for the destination and bounds.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/140_copy_source_validation: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

local dst = ffi.new("char[8]")
local scalar = ffi.new("int", 1)
local nullp = ffi.cast("char *", 0)

-- Accepted zero-length forms.
H.eq(ffi.copy(dst, nil, 0), nil, "nil source with zero length")
H.eq(ffi.copy(dst, nullp, 0), nil, "NULL pointer source with zero length")
H.eq(ffi.copy(dst, "abc", 0), nil, "string source with zero length")

-- Non-pointer sources must be rejected even when nothing is copied.
H.raises(function () ffi.copy(dst, 42, 0) end, "cdata expected",
         "number source")
H.raises(function () ffi.copy(dst, {}, 0) end, "cdata expected",
         "table source")
H.raises(function () ffi.copy(dst, true, 0) end, "cdata expected",
         "boolean source")
H.raises(function () ffi.copy(dst, scalar, 0) end,
         "expected a pointer or array", "scalar cdata source")

-- The same sources with a nonzero length remain rejected.
H.raises(function () ffi.copy(dst, 42, 1) end, "cdata expected",
         "number source with length")
H.raises(function () ffi.copy(dst, scalar, 1) end,
         "expected a pointer or array", "scalar cdata source with length")

-- A valid string copy still includes the NUL by default.
local text = ffi.new("char[8]")
ffi.copy(text, "abc")
H.eq(ffi.string(text), "abc", "default string copy")

print("ffi/140_copy_source_validation ok")
