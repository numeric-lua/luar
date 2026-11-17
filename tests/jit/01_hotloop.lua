-- jit/01_hotloop.lua -- J1 hot-loop recording (no codegen yet).
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
local s = 0
for i = 1, 2000 do s = s + i end
H.eq(s, 2001000, "interpreter result changed")

local st = jit.stats()
H.check(st.hotloops >= 1, "no hot loop recorded")
H.check(st.traces >= 1, "no trace snapshot recorded")
H.eq(st.mcode, 0, "J1 must not generate machine code")
H.eq(st.status, false, "jit.status must remain false")
H.eq(type(st.enabled), "boolean")

local t = jit.trace(1)
H.check(t:find("FORLOOP", 1, true), "trace lacks FORLOOP:\n" .. t)
H.check(t:find("ADD", 1, true), "trace lacks ADD:\n" .. t)
H.check(t:find("MMBIN", 1, true), "trace lacks MMBIN:\n" .. t)

local ir = jit.ir(1)
H.check(ir:find("R%d+ = R%d+ %+ R%d+"), "IR lacks the add:\n" .. ir)
H.check(ir:find("count%("), "IR lacks the loop counter:\n" .. ir)
H.check(ir:find("goto %d+"), "IR lacks the loop target:\n" .. ir)

local all = jit.traces()
H.eq(#all, st.traces, "jit.traces() disagrees with stats")
H.eq(type(all[1]), "string")

-- a backward JMP loop is recorded too (fresh Proto: jit.flush() does not
-- reset Protos that are already hot in J1)
local function while_loop ()
  local n = 0
  while n < 2000 do n = n + 1 end
  return n
end
jit.flush()
H.eq(while_loop(), 2000)
st = jit.stats()
H.check(st.traces >= 1, "while-loop trace missing")
local found = false
for _, tr in ipairs(jit.traces()) do
  if tr:find("JMP", 1, true) then found = true end
end
H.check(found, "no JMP backedge trace")

-- flush resets the recorder
jit.flush()
H.eq(jit.stats().traces, 0)
H.eq(jit.stats().hotloops, 0)

-- jit.off() stops recording
jit.off()
H.eq(jit.stats().enabled, false)
local x = 0
for i = 1, 2000 do x = x + 1 end
H.eq(x, 2000)
H.eq(jit.stats().traces, 0, "off must not record")
jit.on()
H.eq(jit.stats().enabled, true)

-- recording still does not change semantics after re-enabling
jit.flush()
local acc = 0
for i = 1, 100 do acc = acc + i end
H.eq(acc, 5050)
H.eq(jit.status(), false)

print("jit/01_hotloop ok")
