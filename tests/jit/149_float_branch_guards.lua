-- jit/149_float_branch_guards.lua -- branch-loop entry guards must use the
-- operand flavour for float K arithmetic and unary minus.
--
-- The branch guard pass used LUAR_T_INT for ADDK/SUBK/MULK/DIVK and UNM
-- regardless of the decoder's proven type.  A float branch loop therefore
-- compiled successfully but failed its own integer entry guard on every
-- call and silently ran on the interpreter (sig_hits stayed zero).
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/149_float_branch_guards: skipped (no native target)")
  return
end

local function check (name, src, n)
  local f = assert(load(src, "=" .. name))()
  jit.flush()
  jit.opt.start("hotloop=1", "ir=0", "table=0", "call=0")
  jit.off()
  local want = f(n)
  jit.flush()
  jit.on()
  local got = f(n)
  local again = f(n)
  H.eq(got, want, name .. ": first native result")
  H.eq(again, want, name .. ": cached native result")
  local st = jit.stats()
  H.check(st.compiled >= 1, name .. ": loop did not compile")
  H.check(st.sig_hits > 0, name .. ": loop never entered native code")
  H.eq(st.exits, 0, name .. ": loop exited its entry guard")
end

check("addk", [[
  return function (n)
    local s = 0.0
    for i = 1, n do
      if i % 2 == 0 then s = s + 1.5 else s = s - 0.5 end
    end
    return s
  end
]], 100)

check("mulk", [[
  return function (n)
    local s = 1.0
    for i = 1, n do
      if i % 2 == 0 then s = s * 1.5 else s = s / 2.0 end
    end
    return s
  end
]], 100)

-- Float-controlled for loops take the same branch guard path.
check("float-for", [[
  return function (n)
    local s = 0.0
    for i = 1.0, n do
      if i < n * 0.5 then s = s + 0.5 else s = s - 0.5 end
    end
    return s
  end
]], 100)

jit.flush()
print("jit/149_float_branch_guards ok")
