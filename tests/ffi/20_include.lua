-- ffi/20_include.lua -- quoted #include in ffi.cdef.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

local dir = "tests/tmp/ffi-include"
os.execute("mkdir -p " .. dir)
local a = assert(io.open(dir .. "/a.h", "w"))
a:write([[
#ifndef LUAR_INC_A
#define LUAR_INC_A
#define LUAR_INC_N 3
struct IncA { int x; };
#endif
]])
a:close()
local b = assert(io.open(dir .. "/b.h", "w"))
b:write([[
#ifndef LUAR_INC_B
#define LUAR_INC_B
#include "tests/tmp/ffi-include/a.h"
struct IncB { struct IncA a; int y[LUAR_INC_N]; };
#endif
]])
b:close()
local c = assert(io.open(dir .. "/c.h", "w"))
c:write([[
#ifndef LUAR_INC_C
#define LUAR_INC_C
#include "a.h"
struct IncC { struct IncA a; int z; };
#endif
]])
c:close()

ffi.cdef[[
#include "tests/tmp/ffi-include/c.h"
#include "tests/tmp/ffi-include/b.h"
#include "tests/tmp/ffi-include/a.h"
]]
H.eq(ffi.sizeof("struct IncA"), 4)
H.eq(ffi.sizeof("struct IncB"), 16)
H.eq(ffi.sizeof("struct IncC"), 8)

H.raises(function ()
  ffi.cdef('#include "tests/tmp/ffi-include/missing.h"')
end, "cannot open include")
H.raises(function ()
  ffi.cdef("#include <stdio.h>")
end, "only quoted")
ffi.cdef[[
#if 0
#include "tests/tmp/ffi-include/missing.h"
#endif
struct IncSkip { int v; };
]]
H.eq(ffi.sizeof("struct IncSkip"), 4)

print("ffi/20_include ok")
