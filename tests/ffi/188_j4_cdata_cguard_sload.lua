-- ffi/188_j4_cdata_cguard_sload.lua -- a SLOAD of a cdata value must
-- re-check the cdata identity before a field access.
--
-- The old trace kept seen[slot] from an earlier move guard and reused a
-- field offset from a different struct type.  Both types share the
-- USERDATA tag, so only the closure-pointer CGUARD can distinguish them.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/188_j4_cdata_cguard_sload: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int x; } SA;
typedef struct { int y; int x; } SB;
]]

jit.opt.start("ir=1", "hotloop=1", "hotexit=1")

local function fread (x, n)
  local s = 0
  local y = 0
  local z = 0
  for _ = 1, n do
    y = x
    z = y
    y = x
    s = s + y.x
  end
  return s, z
end

local function fwrite (x, n)
  local s = 0
  local y = 0
  local z = 0
  for _ = 1, n do
    y = x
    z = y
    y = x
    y.x = 7
    s = s + 1
  end
  return s
end

local A = ffi.new("SA")
A.x = 42
local B = ffi.new("SB")
B.y = 123
B.x = 99

H.eq(fread(A, 200), 8400, "read SA")
local br = fread(B, 200)
H.eq(br, 19800, "read SB")
H.eq(B.y, 123, "SB.y unchanged by read")
H.eq(B.x, 99, "SB.x unchanged by read")

local C = ffi.new("SB")
C.y = 123
C.x = 99
H.eq(fwrite(A, 200), 200, "write SA")
H.eq(A.x, 7, "SA.x written")
H.eq(fwrite(C, 200), 200, "write SB")
H.eq(C.y, 123, "SB.y not overwritten")
H.eq(C.x, 7, "SB.x written")

jit.opt.start("hotloop=56", "hotexit=10", "ir=0")
print("ffi/188_j4_cdata_cguard_sload ok")
