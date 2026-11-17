-- ffi/21_include_path.lua -- LUAR_FFI_INCLUDE search path.
local H = dofile("tests/helpers.lua")

local dir = "tests/tmp/ffi-include-path"
os.execute("mkdir -p " .. dir)
local h = assert(io.open(dir .. "/path_header.h", "w"))
h:write("struct PathInc { int x; };\n")
h:close()
local s = assert(io.open(dir .. "/check.lua", "w"))
s:write([[
local ffi = require "ffi"
ffi.cdef("#include <path_header.h>")
print(ffi.sizeof("struct PathInc"))
]])
s:close()

local pipe = assert(io.popen(
  "LUAR_FFI_INCLUDE=tests/tmp/nonexistent:" .. dir ..
  " ./src/luar " .. dir .. "/check.lua 2>&1"))
local out = pipe:read("a")
local ok = pipe:close()
H.check(ok, "include-path subprocess failed: " .. tostring(out))
H.eq(out:match("%d+"), "4")

print("ffi/21_include_path ok")
