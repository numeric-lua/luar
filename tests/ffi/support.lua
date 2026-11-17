-- tests/ffi/support.lua -- shared helpers for the FFI suite.
--
-- The test runner also executes this file as a test; that is harmless
-- (it only loads the ffi module and returns a table).

local ffi = require "ffi"

local S = {}

-- Build (once) and load the tiny C helper used for layout and
-- struct-by-value checks.  The .so lives in the gitignored tests/tmp.
function S.helper ()
  local ok, why, code = os.execute("sh tests/ffi/build_helper.sh")
  if not ok then
    error("could not build tests/ffi/helper.c: " ..
          tostring(why) .. " " .. tostring(code))
  end
  return ffi.load("tests/tmp/libluar_ffi_helper.so")
end

return S
