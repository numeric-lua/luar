-- jit/178_repeat_float_k.lua -- fast_repeat must not compare an integer
-- loop index with the integer immediate of a float OP_EQK constant.
--
-- `until i == 3.5` records OP_EQK with the double in the constant table and
-- leaves imm=0.  The fast repeat emitter compared against imm and stopped at
-- i == 0, making an infinite interpreter loop terminate.  The shape must now
-- stay on the interpreter.  A non-integral float can never equal an integer,
-- so the fixed program runs forever; the regression check uses a bounded
-- child process rather than hanging the suite.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/178_repeat_float_k: skipped (no native target)")
  return
end

-- The shape still compiles and is correct for an integer constant.
do
  jit.flush()
  jit.opt.start("hotloop=1", "ir=0")
  jit.on()
  local f = assert(load([[
    return function (n)
      local s, i = 0, n
      repeat i = i - 1; s = s + i until i == 3
      return s, i
    end
  ]], "=repeat_int_k"))()
  local s, i = f(10)
  H.eq(s, 42, "integer repeat sum")
  H.eq(i, 3, "integer repeat index")
  H.check(jit.stats().compiled >= 1, "integer repeat did not compile")
end

-- No-native fallback must still be correct, and no early termination is
-- allowed for the float-K shape.
do
  local timeout = os.execute("command -v timeout >/dev/null 2>&1")
  if not timeout then
    print("jit/178_repeat_float_k: skipped (no timeout command)")
    return
  end
  local path = os.tmpname()
  local out = assert(io.open(path, "w"))
  out:write([=[
    local f = assert(load([[
      return function (n)
        local s, i = 0, n
        repeat i = i - 1; s = s + i until i == 3.5
        return s, i
      end
    ]], "=repeat_float_k"))()
    jit.opt.start("hotloop=1", "ir=0")
    jit.on()
    local s, i = f(10)
    io.write("returned ", tostring(s), " ", tostring(i), "\n")
  ]=])
  out:close()
  local cmd = string.format("timeout 1 ./src/luar %s >/dev/null 2>&1", path)
  local ok = os.execute(cmd)
  os.remove(path)
  H.check(not ok, "fast_repeat stopped at zero for a float constant")
end

jit.flush()
jit.opt.start("hotloop=56", "ir=0")
print("jit/178_repeat_float_k ok")
