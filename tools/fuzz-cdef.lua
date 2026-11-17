-- tools/fuzz-cdef.lua -- deterministic fuzzer for the ffi.cdef parser.
--
-- Usage:
--   ./src/luar tools/fuzz-cdef.lua [--cases N] [--seed N] [--max-seconds S]
--
-- Requires a build with FFI support (make WITH_FFI=1).  Every generated
-- declaration is passed to ffi.cdef() through pcall.  Malformed input is
-- expected to raise a Lua error; what must never happen is a crash, an
-- abort, a hang, a non-string error, or a Lua error escaping the pcall.
--
-- The generator is seeded with a fixed constant and uses splitmix64, so the
-- same arguments produce exactly the same sequence of inputs.  The summary
-- deliberately contains no timings or addresses, which keeps test output
-- deterministic.  The --max-seconds bound is a CPU-time budget for the whole
-- run; if it is exceeded the run fails instead of silently truncating.
--
-- This is a grammar- and mutation-based fuzzer, not a coverage-guided one:
-- it needs no extra dependency and runs in well under a second, which makes
-- it suitable for CI and for ASan/UBSan builds (tests/asan.sh runs it).

local ffi_ok, ffi = pcall(require, "ffi")
if not ffi_ok then
  io.stderr:write("fuzz-cdef: this build has no FFI " ..
                  "(build with WITH_FFI=1)\n")
  os.exit(2)
end

local function die (msg)
  io.stderr:write("fuzz-cdef: " .. msg .. "\n")
  os.exit(2)
end

local function usage ()
  io.stderr:write([[
usage: luar tools/fuzz-cdef.lua [options]

  --cases N        number of inputs to generate (default 5000)
  --seed N         PRNG seed, decimal or 0x... (default 0x5eedc0de)
  --max-seconds S  CPU-time budget for the run (default 4)
  -h, --help       show this help
]])
end

local cases = 5000
local seed = 0x5eedc0de
local max_seconds = 4.0

do
  local i = 1
  while i <= #arg do
    local opt = arg[i]
    if opt == "-h" or opt == "--help" then
      usage()
      os.exit(0)
    elseif opt == "--cases" or opt == "--seed" or opt == "--max-seconds" then
      local value = arg[i + 1]
      if value == nil then die(opt .. " needs a value") end
      if opt == "--max-seconds" then
        local n = tonumber(value)
        if not n or n <= 0 then die("--max-seconds must be a positive number") end
        max_seconds = n
      else
        local n = tonumber(value)
        n = n and math.tointeger(n)
        if not n then die(opt .. " must be an integer") end
        if opt == "--cases" then
          if n < 1 then die("--cases must be at least 1") end
          cases = n
        else
          seed = n
        end
      end
      i = i + 2
    else
      usage()
      die("unknown option: " .. opt)
    end
  end
end

-- Deterministic PRNG (splitmix64).  Lua 5.5 integers are 64-bit and wrap on
-- overflow, which is exactly what the reference splitmix64 does.
local rng_state = seed
local function rng ()
  rng_state = rng_state + 0x9e3779b97f4a7c15
  local z = rng_state
  z = (z ~ (z >> 30)) * 0xbf58476d1ce4e5b9
  z = (z ~ (z >> 27)) * 0x94d049bb133111eb
  return z ~ (z >> 31)
end

-- Uniform integer in [1, n]; use unsigned high bits so n is never negative.
local function rnd (n)
  return (rng() >> 33) % n + 1
end

local function choose (list)
  return list[rnd(#list)]
end

-- Keep individual inputs small: the parser must reject oversized garbage
-- quickly, and a fuzzer case should never turn into a stress test.
local MAX_LEN = 1024
local function cap (s)
  if #s > MAX_LEN then return s:sub(1, MAX_LEN) end
  return s
end

-- Known-tricky seeds.  They cover the constructs most likely to expose a
-- parser bug: nested arrays, anonymous structs/unions, unterminated
-- strings/comments, huge integer constants, and long pointer chains.
-- The fuzzer mutates and truncates these as well as generating fresh soup.
local seeds = {
  "struct NestedArrays { int a[2][3][4]; char b[1][2][3][4]; };",
  "typedef int Matrix[4][4]; typedef Matrix Cube[4][4][4];",
  "struct Outer { int tag; union { int i; double d; }; " ..
    "struct { short s; unsigned char b; }; };",
  "struct { struct { int x; }; };",
  "typedef struct { struct { union { int a; float b; }; }; int z; } Anon;",
  "/* unterminated comment",
  "/* closed */ int x; // line comment without a newline",
  "int x = \"unterminated string;",
  "int x[18446744073709551615];",
  "int x[9223372036854775807];",
  "int x[0xffffffffffffffff];",
  "enum Huge { H0 = 0xffffffffffffffff, H1 = 999999999999999999999 };",
  "int x = 0xffffffffffffffffffffffffffffffff;",
  "int x[1e999];",
  "int " .. string.rep("*", 64) .. "p;",
  "int " .. string.rep("*", 1024) .. "q;",
  "int " .. string.rep("(", 64) .. "x" .. string.rep(")", 64) .. ";",
  "int (*(*(*(*f)(int))(int))(int))(int);",
  "typedef struct Node { struct Node *next; int v; } Node;",
  "struct Fwd; struct Use { struct Fwd *p; }; struct Fwd { int x; };",
  "int f(void); int g(int, ...); int (*cb)(const char *, void *);",
  "enum { A = 1 << 62, B = A / 2, C = A % 7, D = 1 / 0 };",
  "struct Bits { int x : 3; };",
  "long double d;",
  "#define X 1\nint x;",
  "int x[0]; int y[?]; int z[];",
  "struct S { int a; } s; int a, *b, c[4];",
  "typedef const volatile unsigned int cv; typedef int (*BinOp)(int, int);",
  "struct Deep { struct { struct { struct { int x; } c; } b; } a; };",
}

local keywords = {
  "typedef", "struct", "union", "enum", "int", "char", "short", "long",
  "unsigned", "signed", "void", "float", "double", "const", "volatile",
  "restrict", "static", "extern", "inline", "register", "auto", "_Bool",
  "bool", "size_t", "ssize_t", "int8_t", "uint64_t", "intptr_t",
  "__attribute__", "__declspec", "_Atomic", "_Complex", "__int128",
  "asm", "_Static_assert", "static_assert", "thistype", "global",
}

local identifiers = {
  "foo", "bar", "Baz", "x", "y", "T", "S", "E", "u8", "Node", "__x",
  "_", "intx", "structx", "name_with_a_fairly_long_identifier",
}

local numbers = {
  "0", "1", "2", "7", "42", "255", "2147483647", "4294967295",
  "9223372036854775807", "18446744073709551615", "0x0", "0x7fffffff",
  "0xffffffffffffffff", "0x100000000", "1e9", "1e999", "0x", "09",
  "1ul", "1ULL", "0b1010", "123456789012345678901234567890",
}

local punctuators = {
  "*", "(", ")", "[", "]", "{", "}", ";", ",", ":", "=", "+", "-",
  "/", "%", "&", "|", "^", "~", "!", "<", ">", ".", "?", "...", "<<",
  ">>", "<=", ">=", "==", "!=", "&&", "||", "->", "#", "@", "$",
}

local comments = {
  "/* */", "/**/", "/*", "*/", "//", "// x\n", "/* x */", "#define X 1",
  "#include <stdio.h>\n", "/* unterminated", "\"unterminated",
}

local separators = { " ", " ", " ", "", "\n", "\t", "/* c */" }

local tokens = {}
for _, group in ipairs({keywords, identifiers, numbers, punctuators, comments}) do
  for _, tok in ipairs(group) do tokens[#tokens + 1] = tok end
end

local function random_soup ()
  local n = rnd(28)
  local out = choose(tokens)
  for _ = 2, n do
    out = out .. choose(separators) .. choose(tokens)
    if #out > MAX_LEN then break end
  end
  return cap(out)
end

local alphabet = "abcdefghijklmnopqrstuvwxyz" ..
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ..
                 "*()[]{};,:=+-/%&|^~!<>? \n\t\"'/*"

local function mutate (s)
  for _ = 1, rnd(5) do
    local len = #s
    local op = rnd(7)
    if op == 1 then
      -- Truncate at a random byte, including the empty string.
      s = s:sub(1, rnd(len + 1) - 1)
    elseif op == 2 and len > 0 then
      -- Delete one byte.
      local pos = rnd(len)
      s = s:sub(1, pos - 1) .. s:sub(pos + 1)
    elseif op == 3 then
      -- Insert a random token.
      local pos = rnd(len + 1)
      s = s:sub(1, pos - 1) .. choose(tokens) .. s:sub(pos)
    elseif op == 4 and len > 0 then
      -- Duplicate one byte.
      local pos = rnd(len)
      local c = s:sub(pos, pos)
      s = s:sub(1, pos) .. c .. s:sub(pos + 1)
    elseif op == 5 and len > 0 then
      -- Replace one byte.
      local pos = rnd(len)
      local at = rnd(#alphabet)
      s = s:sub(1, pos - 1) .. alphabet:sub(at, at) .. s:sub(pos + 1)
    elseif op == 6 then
      -- Splice another token at the end.
      s = s .. choose(separators) .. choose(tokens)
    else
      -- Drop the final byte, a cheap way to test truncation paths.
      s = s:sub(1, len - 1)
    end
    s = cap(s)
  end
  return s
end

local fresh_counter = 0
local function fresh (prefix)
  fresh_counter = fresh_counter + 1
  return prefix .. fresh_counter
end

-- Bounded recursive generator for syntactically plausible aggregates.  The
-- depth limit keeps generated inputs far away from the C stack limits that
-- huge hand-written nesting can hit.
local function gen_type (depth)
  local pick = rnd(6)
  if depth <= 0 or pick <= 2 then
    return choose({
      "int", "char", "unsigned long", "double", "void *", "int8_t",
      "size_t", "struct Fwd *", "const char *", "union U *",
    })
  elseif pick == 3 then
    return gen_type(depth - 1) .. " *"
  elseif pick == 4 then
    return gen_type(depth - 1) .. "[" .. choose({"1", "2", "4", "0", "?", "-1",
                                                 "18446744073709551615"}) .. "]"
  elseif pick == 5 then
    return gen_type(depth - 1) .. " (*)(" ..
           choose({"void", "int", "const char *", "..."}) .. ")"
  else
    return "struct { " .. gen_type(depth - 1) .. " " .. fresh("m") .. "; }"
  end
end

local function gen_aggregate (depth)
  local keyword = choose({"struct", "union"})
  local tag = ""
  if rnd(2) == 1 then tag = " " .. fresh("Fz") end
  local members = {}
  for i = 1, rnd(4) do
    if depth > 0 and rnd(4) == 1 then
      members[i] = gen_aggregate(depth - 1) .. " " .. fresh("m") .. ";"
    else
      local suffix = choose({"", "", "[" .. choose({"1", "2", "?",
                                                   "18446744073709551615"}) .. "]"})
      members[i] = gen_type(depth) .. " " .. fresh("f") .. suffix .. ";"
    end
  end
  local body = table.concat(members, " ")
  if rnd(2) == 1 then
    return keyword .. tag .. " { " .. body .. " };"
  end
  return "typedef " .. keyword .. tag .. " { " .. body .. " } " ..
         fresh("T") .. ";"
end

local function gen_enum ()
  local items = {}
  for i = 1, rnd(4) do
    local value = ""
    if rnd(2) == 1 then
      value = " = " .. choose({"0", "1", "-1", "1 << 4", "0xffffffffffffffff",
                               "(1 + 2) * 3", "1 / 0", "999999999999999999999"})
    end
    items[i] = fresh("E") .. value
  end
  return "enum " .. fresh("Ez") .. " { " .. table.concat(items, ", ") .. " };"
end

local function make_case (i)
  if i <= #seeds then return seeds[i] end
  local mode = i % 6
  if mode == 0 then
    return mutate(seeds[rnd(#seeds)])
  elseif mode == 1 then
    local s = seeds[rnd(#seeds)]
    return s:sub(1, rnd(#s + 1) - 1)
  elseif mode == 2 then
    return random_soup()
  elseif mode == 3 then
    return gen_aggregate(rnd(3))
  elseif mode == 4 then
    return gen_enum() .. " " .. random_soup()
  else
    local a = seeds[rnd(#seeds)]
    local b = seeds[rnd(#seeds)]
    local cut = rnd(#a + 1) - 1
    return cap(a:sub(1, cut) .. b:sub(rnd(#b + 1)))
  end
end

local started = os.clock()
local errors, successes = 0, 0

for i = 1, cases do
  local src = make_case(i)
  local ok, err = pcall(ffi.cdef, src)
  if ok then
    successes = successes + 1
  else
    if type(err) ~= "string" then
      io.stderr:write(("fuzz-cdef: FAIL case %d: non-string error: %s\n"):format(
        i, tostring(err)))
      io.stderr:write(("fuzz-cdef: input: %q\n"):format(src))
      os.exit(1)
    end
    errors = errors + 1
  end
  if os.clock() - started > max_seconds then
    io.stderr:write(
      ("fuzz-cdef: FAIL: %.1fs time bound exceeded at case %d\n"):format(
        max_seconds, i))
    os.exit(1)
  end
end

io.write(
  ("fuzz-cdef: %d cases ok (%d cdef errors, %d accepted, seed 0x%x)\n"):format(
    cases, errors, successes, seed))
