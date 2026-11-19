-- jit/62_exit_spill.lua -- full-width snapshot/restore and TSET fallback.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/62_exit_spill: skipped (no native target)")
  return
end

local ir = jit.trace_ir()
H.eq(ir.snapshot_selftest(), true)

jit.flush()
jit.opt.start("ir=1")
jit.on()
-- Growing writes cannot use the TSET side path; they must fall back safely.
local t = {}
local function fill (t, n)
  local s = 0
  for i = 1, n do
    t[i] = i * 2
    s = s + t[i]
  end
  return s
end
H.eq(fill(t, 500), 500 * 501)
for i = 1, 500 do H.eq(t[i], i * 2) end

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/62_exit_spill ok")
