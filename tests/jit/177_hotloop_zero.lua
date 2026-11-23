-- jit/177_hotloop_zero.lua -- hotloop=0 disables tracing for new Protos.
--
-- The documentation defines hotloop=0 as the off switch.  It used to be
-- rejected by jit.opt.start, so newly loaded loops still reached the
-- default threshold and compiled.  Functions are created with load() after
-- the option is set because a chunk's Protos are created when it loads.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/177_hotloop_zero: skipped (no native target)")
  return
end

local ok, err = pcall(function ()
  jit.flush()
  jit.opt.start("hotloop=0")
  jit.on()

  local offloop = assert(load([[
    return function (n)
      local s = 0
      for i = 1, n do s = s + i end
      return s
    end
  ]], "=hotloop0"))()

  H.eq(offloop(100000), 100000 * 100001 // 2, "hotloop=0 result")
  local st = jit.stats()
  H.eq(st.compiled, 0, "hotloop=0 compiled a loop")
  H.eq(st.hotloops, 0, "hotloop=0 reached the hot hook")

  -- A Proto created after restoring the threshold compiles normally.
  jit.opt.start("hotloop=1")
  jit.flush()
  local onloop = assert(load([[
    return function (n)
      local s = 0
      for i = 1, n do s = s + i end
      return s
    end
  ]], "=hotloop1"))()
  H.eq(onloop(1000), 1000 * 1001 // 2, "restored threshold result")
  H.check(jit.stats().compiled >= 1, "restored threshold did not compile")
end)

jit.opt.start("hotloop=56")
jit.flush()
if not ok then error(err, 0) end
print("jit/177_hotloop_zero ok")
