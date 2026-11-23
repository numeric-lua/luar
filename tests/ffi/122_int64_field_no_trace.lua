-- ffi/122_int64_field_no_trace.lua -- 64-bit struct field reads must stay
-- on the interpreter.
--
-- A `long`/`int64_t` field read produces cdata int64, not a Lua integer, so
-- a tracing IR that treats it as an integer can store the wrong register
-- and silently miscompile `x = x + s.z`.  The field recorder now rejects
-- 8-byte integer members, exactly as the existing hot-loop test documents.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/122_int64_field_no_trace: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/122_int64_field_no_trace: skipped (no native target)")
  return
end

-- Load the functions after setting hotloop so their Protos really carry the
-- low threshold; a chunk defined before jit.opt.start would keep the
-- default and never exercise the bug.
jit.flush()
jit.opt.start("ir=1", "hotloop=1")
jit.on()
local maker = assert(load([=[
  local ffi = require "ffi"
  ffi.cdef[[
  struct LuarInt64Field { int x; double y; long z; };
  ]]
  local s = ffi.new("struct LuarInt64Field")
  s.z = 1234567890123
  local function sum_z (o, n)
    local x = 0
    for _ = 1, n do x = x + o.z end
    return x
  end
  return s, sum_z
]=], "=int64_field"))
local s, sum_z = maker()

jit.off()
local want = sum_z(s, 1000)
jit.on()
jit.flush()
jit.opt.start("ir=1", "hotloop=1")
local before = jit.stats().ir_compiled
local got = sum_z(s, 1000)
local after = jit.stats().ir_compiled

H.eq(tostring(got), tostring(want), "64-bit field read result")
H.eq(tostring(got), "1234567890123000LL", "64-bit field read value")
H.eq(after, before, "an 8-byte field read must not produce a trace")

-- A 32-bit sibling field still traces, proving the rejection is size-based
-- rather than disabling the whole struct.
local maker32 = assert(load([=[
  local ffi = require "ffi"
  ffi.cdef[[
  struct LuarInt32Field { int x; double y; long z; };
  ]]
  local s = ffi.new("struct LuarInt32Field")
  s.x = 7
  local function sum_x (o, n)
    local x = 0
    for _ = 1, n do x = x + o.x end
    return x
  end
  return s, sum_x
]=], "=int32_field"))
local s32, sum_x = maker32()
jit.flush()
jit.opt.start("ir=1", "hotloop=1")
local before32 = jit.stats().ir_compiled
H.eq(sum_x(s32, 1000), 7000)
H.check(jit.stats().ir_compiled > before32,
        "32-bit field read should still trace")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/122_int64_field_no_trace ok")
