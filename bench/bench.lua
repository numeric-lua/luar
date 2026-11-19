#!/usr/bin/env lua
--[[--------------------------------------------------------------------
bench/bench.lua -- Luar microbenchmark harness and corpus.

The corpus is written in portable Lua 5.1/5.5 so the same file runs under
stock Lua 5.5.1, LuaJIT 2.1 (JIT on and off) and Luar.  Small workloads
have fixed sizes tuned for roughly 20-35 ms per run on an ordinary x86-64
interpreter; --scale changes them for other machines.

Usage:
  lua bench/bench.lua [all|GROUP|NAME ...] [options]

Options:
  --bench NAME[,NAME...]  select benchmarks (repeatable).  NAME may be an
                          exact benchmark, a group, or "all".
  --repeat N              timed samples per benchmark (default 5).
  --warmup N              warmup runs per benchmark (default 3).
  --scale F               multiply workload sizes (default 1.0).
  --json                  emit machine-readable JSON instead of a table.
  --baseline PATH         JSON emitted by a stock-Lua run; adds a
                          "vs lua" slowdown column.
  --label NAME            label for this interpreter (default: auto).
  --wall MODE             auto|date|uptime|clock (default: auto).
  --report FILE [...]     merge JSON files and print Markdown; no timing.
  --list                  list the corpus and exit.
  --help                  this text.

The harness warms up, picks a batch size so one sample lasts about 20 ms,
then takes N timed samples.  It reports the best and the median CPU time
per run, plus wall time for the whole batch.  Benchmarks that fail are
guarded with pcall and reported as "error"; benchmarks whose feature is
absent are reported as "skipped".  There are no pass/fail gates.

This file is a benchmark, not a test: it always exits 0 unless the harness
itself cannot run (bad option, unreadable baseline/report file).

See bench/README.md for methodology and caveats.
--------------------------------------------------------------------]]

local floor = math.floor
local max = math.max
local sort = table.sort
local concat = table.concat
local format = string.format

local SCALE = 1.0

local function N (base)
  local v = floor(base * SCALE + 0.5)
  if v < 1 then v = 1 end
  return v
end


-- ---------------------------------------------------------------- --
-- Feature detection                                                 --
-- ---------------------------------------------------------------- --

local function probe_table_new ()
  if type(table.new) == "function" then
    return table.new, "global"
  end
  local ok, f = pcall(require, "table.new")
  if ok and type(f) == "function" then
    return f, "require"
  end
  return nil, nil
end


local function probe_table_clear ()
  if type(table.clear) == "function" then
    return table.clear, "global"
  end
  local ok, f = pcall(require, "table.clear")
  if ok and type(f) == "function" then
    return f, "require"
  end
  return nil, nil
end


local tnew, tnew_src = probe_table_new()
local tclear, tclear_src = probe_table_clear()
local tcreate = type(table.create) == "function" and table.create or nil
local string_pack = type(string.pack) == "function" and string.pack or nil


local function new_clear_note ()
  if tnew and tclear then
    return "using table.new/table.clear (via " .. tnew_src .. "/" .. tclear_src .. ")"
  elseif tcreate and tclear then
    return "fallback: table.create + table.clear"
  elseif tcreate then
    return "fallback: table.create + pairs()-based clear (table.clear unavailable)"
  elseif tnew and tclear then
    return "using table.new/table.clear"
  else
    return "fallback: plain {} + pairs()-based clear (table.new/table.clear unavailable)"
  end
end


-- ---------------------------------------------------------------- --
-- JSON (minimal encoder/decoder; enough for our own result files)   --
-- ---------------------------------------------------------------- --

local function json_escape (s)
  local map = {
    ['"'] = '\\"', ['\\'] = '\\\\', ['\b'] = '\\b', ['\f'] = '\\f',
    ['\n'] = '\\n', ['\r'] = '\\r', ['\t'] = '\\t',
  }
  return (s:gsub('[%c\\"]', function (c)
    local e = map[c]
    if e then return e end
    return format("\\u%04x", string.byte(c))
  end))
end


local function json_number (x)
  if x ~= x or x == math.huge or x == -math.huge then return "null" end
  if x == floor(x) and math.abs(x) < 1e15 then
    return format("%d", x)
  end
  return format("%.17g", x)
end


local function json_encode (root)
  local out = {}
  local emit
  emit = function (v)
    local t = type(v)
    if t == "nil" then
      out[#out + 1] = "null"
    elseif t == "boolean" then
      out[#out + 1] = v and "true" or "false"
    elseif t == "number" then
      out[#out + 1] = json_number(v)
    elseif t == "string" then
      out[#out + 1] = '"' .. json_escape(v) .. '"'
    elseif t == "table" then
      local n = #v
      if n > 0 or next(v) == nil then
        out[#out + 1] = "["
        for i = 1, n do
          if i > 1 then out[#out + 1] = "," end
          emit(v[i])
        end
        out[#out + 1] = "]"
      else
        local keys = {}
        for k in pairs(v) do
          if type(k) == "string" then keys[#keys + 1] = k end
        end
        sort(keys)
        out[#out + 1] = "{"
        for i, k in ipairs(keys) do
          if i > 1 then out[#out + 1] = "," end
          emit(k)
          out[#out + 1] = ":"
          emit(v[k])
        end
        out[#out + 1] = "}"
      end
    else
      error("cannot JSON-encode a " .. t)
    end
  end
  emit(root)
  return concat(out)
end


local function json_decode (text)
  local pos = 1
  local function fail (msg)
    error(format("JSON %s near byte %d", msg, pos), 0)
  end

  local function skip ()
    while true do
      local c = text:sub(pos, pos)
      if c == " " or c == "\t" or c == "\r" or c == "\n" then
        pos = pos + 1
      else
        return
      end
    end
  end

  local parse_value

  local function parse_string ()
    pos = pos + 1                      -- opening quote
    local buf = {}
    local escapes = {
      ['"'] = '"', ['\\'] = '\\', ['/'] = '/', b = '\b', f = '\f',
      n = '\n', r = '\r', t = '\t',
    }
    while true do
      local c = text:sub(pos, pos)
      if c == "" then fail("unterminated string") end
      if c == '"' then
        pos = pos + 1
        return concat(buf)
      elseif c == "\\" then
        local e = text:sub(pos + 1, pos + 1)
        if escapes[e] then
          buf[#buf + 1] = escapes[e]
          pos = pos + 2
        elseif e == "u" then
          local hex = text:sub(pos + 2, pos + 5)
          if #hex < 4 or hex:find("[^0-9a-fA-F]") then
            fail("bad \\u escape")
          end
          local cp = tonumber(hex, 16)
          if cp < 0x80 then
            buf[#buf + 1] = string.char(cp)
          elseif cp < 0x800 then
            buf[#buf + 1] = string.char(0xC0 + floor(cp / 0x40),
                                        0x80 + cp % 0x40)
          else
            buf[#buf + 1] = string.char(0xE0 + floor(cp / 0x1000),
                                        0x80 + floor(cp / 0x40) % 0x40,
                                        0x80 + cp % 0x40)
          end
          pos = pos + 6
        else
          fail("bad escape")
        end
      else
        buf[#buf + 1] = c
        pos = pos + 1
      end
    end
  end

  local function parse_number ()
    local tok = text:sub(pos):match("^-?%d+%.?%d*[eE]?[-+]?%d*")
    if not tok then fail("bad number") end
    local n = tonumber(tok)
    if not n then fail("bad number") end
    pos = pos + #tok
    return n
  end

  local function parse_array ()
    pos = pos + 1
    local arr = {}
    skip()
    if text:sub(pos, pos) == "]" then
      pos = pos + 1
      return arr
    end
    while true do
      arr[#arr + 1] = parse_value()
      skip()
      local c = text:sub(pos, pos)
      if c == "," then
        pos = pos + 1
      elseif c == "]" then
        pos = pos + 1
        return arr
      else
        fail("expected ',' or ']'")
      end
    end
  end

  local function parse_object ()
    pos = pos + 1
    local obj = {}
    skip()
    if text:sub(pos, pos) == "}" then
      pos = pos + 1
      return obj
    end
    while true do
      skip()
      if text:sub(pos, pos) ~= '"' then fail("expected string key") end
      local k = parse_string()
      skip()
      if text:sub(pos, pos) ~= ":" then fail("expected ':'") end
      pos = pos + 1
      obj[k] = parse_value()
      skip()
      local c = text:sub(pos, pos)
      if c == "," then
        pos = pos + 1
      elseif c == "}" then
        pos = pos + 1
        return obj
      else
        fail("expected ',' or '}'")
      end
    end
  end

  parse_value = function ()
    skip()
    local c = text:sub(pos, pos)
    if c == "{" then
      return parse_object()
    elseif c == "[" then
      return parse_array()
    elseif c == '"' then
      return parse_string()
    elseif c == "t" then
      if text:sub(pos, pos + 3) == "true" then
        pos = pos + 4
        return true
      end
    elseif c == "f" then
      if text:sub(pos, pos + 4) == "false" then
        pos = pos + 5
        return false
      end
    elseif c == "n" then
      if text:sub(pos, pos + 3) == "null" then
        pos = pos + 4
        return nil
      end
    elseif c == "-" or c:match("%d") then
      return parse_number()
    end
    fail("unexpected value")
  end

  local v = parse_value()
  skip()
  if pos <= #text then fail("trailing data") end
  return v
end


local function read_file (path)
  local f, err = io.open(path, "rb")
  if not f then return nil, err end
  local s = f:read("*a")
  f:close()
  return s
end


-- ---------------------------------------------------------------- --
-- Wall clock                                                       --
-- ---------------------------------------------------------------- --

local function date_now ()
  local ok, p = pcall(io.popen, "date +%s%N 2>/dev/null")
  if not ok or not p then return nil end
  local s = p:read("*l")
  p:close()
  local n = s and tonumber(s)
  if n and n > 1e12 then return n / 1e9 end
  return nil
end


local function uptime_now ()
  local f = io.open("/proc/uptime", "r")
  if not f then return nil end
  local s = f:read("*l")
  f:close()
  local n = s and tonumber(s:match("^(%d+%.?%d*)"))
  return n
end


local function resolve_wall (mode)
  if mode == "clock" then
    return os.clock, "clock"
  elseif mode == "uptime" then
    return uptime_now, "uptime"
  elseif mode == "date" then
    return date_now, "date"
  elseif mode == "auto" then
    if date_now() then return date_now, "date" end
    if uptime_now() then return uptime_now, "uptime" end
    return os.clock, "clock"
  end
  error("unknown wall clock mode: " .. tostring(mode))
end


-- ---------------------------------------------------------------- --
-- Interpreter / host metadata                                      --
-- ---------------------------------------------------------------- --

local function detect_kind ()
  if type(luar) == "table" and luar.version then return "luar" end
  if type(jit) == "table" and type(jit.status) == "function" then
    return "luajit"
  end
  return "lua"
end


local function collect_meta (opts, wall_kind)
  local kind = detect_kind()
  local version
  if kind == "luar" then
    version = "Luar " .. tostring(luar.version) .. " (" ..
              tostring(luar.lua_version or _VERSION) .. ")"
  elseif kind == "luajit" then
    version = tostring(jit.version)
  else
    version = tostring(_VERSION)
  end

  local jit_on, jit_flags = false, ""
  if type(jit) == "table" and type(jit.status) == "function" then
    local res = { pcall(jit.status) }
    if res[1] then
      jit_on = res[2] and true or false
      if jit_on then
        local flags = {}
        for i = 3, #res do flags[#flags + 1] = tostring(res[i]) end
        jit_flags = concat(flags, " ")
      end
    end
  end

  local model, ncpu = "", 0
  local f = io.open("/proc/cpuinfo", "r")
  if f then
    for line in f:lines() do
      if line:match("^processor%s*:") then ncpu = ncpu + 1 end
      local m = line:match("^model name%s*:%s*(.+)$")
      if m and model == "" then
        model = m:match("^%s*(.-)%s*$")
      end
    end
    f:close()
  end

  local platform = "unknown"
  if type(jit) == "table" and jit.os and jit.arch then
    platform = tostring(jit.os) .. " " .. tostring(jit.arch)
  end

  return {
    schema = "luar-bench-v1",
    label = opts.label or kind,
    kind = kind,
    version = version,
    lua_version = tostring(_VERSION),
    jit = jit_on,
    jit_flags = jit_flags,
    platform = platform,
    cpu = model,
    nproc = ncpu,
    date = os.date("%Y-%m-%dT%H:%M:%S%z"),
    reps = opts.reps,
    warmup = opts.warmup,
    scale = opts.scale,
    wall_clock = wall_kind or "none",
    features = {
      table_new = tnew ~= nil,
      table_new_source = tnew_src or "none",
      table_clear = tclear ~= nil,
      table_clear_source = tclear_src or "none",
      table_create = tcreate ~= nil,
      string_pack = string_pack ~= nil,
    },
    baseline = opts.baseline or false,
  }
end


-- ---------------------------------------------------------------- --
-- Corpus helpers                                                   --
-- ---------------------------------------------------------------- --

local function fib (n)
  if n < 2 then return n end
  return fib(n - 1) + fib(n - 2)
end


local method_obj = { v = 0 }
function method_obj:add (x)
  self.v = self.v + x
end

local meta_obj = setmetatable({}, {
  __index = function (_, k) return k * 2 end,
})

local gsub_text = string.rep("abc123 ", 100)

local hash_keys = {}
local function prepare_hash ()
  hash_keys = {}
  local n = N(60000)
  for i = 1, n do hash_keys[i] = "key:" .. i end
end


-- table.new/table.clear (or nearest fallback, guarded by feature probes).
local clear_impl = tclear or function (t)
  for k in pairs(t) do t[k] = nil end
end

local new_impl = tnew or tcreate or function () return {} end


-- ---------------------------------------------------------------- --
-- The corpus                                                       --
-- ---------------------------------------------------------------- --

local BENCH = {}
local function def (t) BENCH[#BENCH + 1] = t end


-- numeric -----------------------------------------------------------

def {
  name = "numeric/int_sum", group = "numeric",
  run = function ()
    local n = N(8000000)
    local s = 0
    for i = 1, n do s = s + i end
    return s
  end,
}

def {
  name = "numeric/float_loop", group = "numeric",
  run = function ()
    local n = N(4000000)
    local x = 0.0
    for i = 1, n do x = x + i * 0.25 end
    return x
  end,
}

def {
  name = "numeric/product", group = "numeric",
  run = function ()
    local n = N(4000000)
    local s = 1
    for i = 1, n do s = s * i end
    return s
  end,
}

def {
  name = "numeric/lin_sum", group = "numeric",
  run = function ()
    local n = N(8000000)
    local s = 0
    for i = 1, n do s = s + i * 3 end
    return s
  end,
}

def {
  name = "numeric/square_sum", group = "numeric",
  run = function ()
    local n = N(8000000)
    local s = 0
    for i = 1, n do s = s + i * i end
    return s
  end,
}

def {
  name = "numeric/repeat_sum", group = "numeric",
  run = function ()
    local n = N(8000000)
    local i, s = 0, 0
    repeat i = i + 1; s = s + i until i >= n
    return s
  end,
}

def {
  name = "numeric/while_sum", group = "numeric",
  run = function ()
    local n = N(8000000)
    local i, s = 0, 0
    while i < n do s = s + i; i = i + 1 end
    return s
  end,
}

def {
  name = "numeric/float_div", group = "numeric",
  run = function ()
    local n = N(2000000)
    local s = 1.0
    for i = 1, n do s = s / i end
    return s
  end,
}

def {
  name = "numeric/float_sum", group = "numeric",
  run = function ()
    local n = N(2000000)
    local s = 0.0
    for i = 1, n do s = s + i end
    return s
  end,
}

def {
  name = "numeric/fib", group = "numeric",
  run = function ()
    return fib(27)
  end,
}

def {
  name = "numeric/mandel", group = "numeric",
  run = function ()
    local w = N(320)
    local c = 0
    for y = 0, w - 1 do
      local ci = (y - w / 2) / 55
      for x = 0, w - 1 do
        local cr = (x - w * 0.65) / 55
        local zr, zi = 0.0, 0.0
        local i = 0
        while i < 60 and zr * zr + zi * zi < 4.0 do
          zr, zi = zr * zr - zi * zi + cr, 2 * zr * zi + ci
          i = i + 1
        end
        c = c + i
      end
    end
    return c
  end,
}

-- table -------------------------------------------------------------

def {
  name = "table/array", group = "table",
  run = function ()
    local n = N(1400000)
    local a = {}
    for i = 1, n do a[i] = i end
    local s = 0
    for i = 1, n do s = s + a[i] end
    return s
  end,
}

def {
  name = "table/hash", group = "table",
  prepare = prepare_hash,
  run = function ()
    local keys = hash_keys
    local n = #keys
    local t = {}
    for i = 1, n do t[keys[i]] = i end
    local s = 0
    for i = 1, n do s = s + t[keys[i]] end
    return s
  end,
}

def {
  name = "table/new_clear", group = "table",
  note = new_clear_note(),
  run = function ()
    local n = N(500000)
    local t = new_impl(n, 0)
    for i = 1, n do t[i] = i end
    clear_impl(t)
    for i = 1, n do t[i] = i end
    return #t
  end,
}

-- j4 (Luar tracing IR) ---------------------------------------------

local function j4_skip ()
  if type(luar) ~= "table" or type(jit) ~= "table" then
    return "J4 requires Luar"
  end
  local ok = pcall(function () jit.opt.start("ir=1") end)
  if not ok then
    return "J4 IR option is not available"
  end
  return nil
end

local function j4_prepare ()
  jit.opt.start("ir=1")
  jit.on()
end

local j4_array, j4_hash, j4_a, j4_b

local function j4_make_array (n)
  jit.off()
  local t = {}
  for i = 1, n do t[i] = i end
  jit.on()
  return t
end

local function j4_make_hash (n)
  jit.off()
  local t = table.new(0, n)
  for i = 1, n do t[i] = i end
  jit.on()
  return t
end

local function j4_dbl (x) return x * 2 end
local function j4_add2 (x, y) return x + y end

local function j4_read (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

local function j4_mixed (t, f, n)
  local s = 0
  for i = 1, n do s = s + t[i] + f(i) end
  return s
end

def {
  name = "j4/int_sum", group = "j4",
  skip = j4_skip,
  prepare = j4_prepare,
  run = function ()
    local n = N(8000000)
    local s = 0
    for i = 1, n do s = s + i end
    return s
  end,
}

def {
  name = "j4/table_read", group = "j4",
  skip = j4_skip,
  prepare = function ()
    j4_array = j4_make_array(N(1400000))
    j4_prepare()
  end,
  run = function ()
    local a = j4_array
    return j4_read(a, #a)
  end,
}

def {
  name = "j4/table_hash_side", group = "j4",
  skip = j4_skip,
  prepare = function ()
    j4_array = j4_make_array(N(1000000))
    j4_hash = j4_make_hash(N(1000000))
    j4_prepare()
  end,
  run = function ()
    local n = #j4_array
    return j4_read(j4_array, n) + j4_read(j4_hash, n)
  end,
}

def {
  name = "j4/float_table", group = "j4",
  skip = j4_skip,
  prepare = function ()
    jit.off()
    local t = {}
    for i = 1, N(1000000) do t[i] = i + 0.5 end
    j4_array = t
    j4_prepare()
  end,
  run = function ()
    local t = j4_array
    local s = 0
    for i = 1, #t do s = s + t[i] * 2 end
    return s
  end,
}

def {
  name = "j4/hash_insert", group = "j4",
  skip = j4_skip,
  prepare = function ()
    jit.off()
    j4_hash = {}
    j4_prepare()
  end,
  run = function ()
    local t = j4_hash
    local n = N(200000)
    local s = 0
    for i = 1, n do
      t[i] = i
      s = s + t[i]
    end
    return s
  end,
}

def {
  name = "j4/call_leaf", group = "j4",
  skip = j4_skip,
  prepare = j4_prepare,
  run = function ()
    local n = N(1500000)
    local f = j4_dbl
    local s = 0
    for i = 1, n do s = s + f(i) end
    return s
  end,
}

def {
  name = "j4/call_exit", group = "j4",
  skip = j4_skip,
  prepare = j4_prepare,
  run = function ()
    local n = N(1500000)
    local y = 10
    local function f (x) return x + y end
    local function g (fn, n)
      local s = 0
      for i = 1, n do s = s + fn(i) end
      return s
    end
    return g(f, n)
  end,
}

def {
  name = "j4/call_twoarg", group = "j4",
  skip = j4_skip,
  prepare = j4_prepare,
  run = function ()
    local n = N(1500000)
    local f = j4_add2
    local s = 0
    for i = 1, n do s = s + f(i, 1) end
    return s
  end,
}

def {
  name = "j4/multi_table", group = "j4",
  skip = j4_skip,
  prepare = function ()
    j4_a = j4_make_array(N(1000000))
    j4_b = j4_make_array(N(1000000))
    j4_prepare()
  end,
  run = function ()
    return j4_mixed(j4_a, j4_dbl, #j4_a) +
           j4_mixed(j4_b, j4_dbl, #j4_b)
  end,
}

-- string ------------------------------------------------------------

def {
  name = "string/concat", group = "string",
  run = function ()
    local n = N(20000)
    local s = ""
    for i = 1, n do s = s .. "x" end
    return #s
  end,
}

def {
  name = "string/format", group = "string",
  run = function ()
    local n = N(50000)
    local len = 0
    for i = 1, n do
      len = len + #format("%d:%s:%.3f", i, "abc", i * 0.5)
    end
    return len
  end,
}

def {
  name = "string/gsub", group = "string",
  run = function ()
    local n = N(2000)
    local len = 0
    for i = 1, n do
      len = len + #gsub_text:gsub("(%a+)(%d+)", "%2%1")
    end
    return len
  end,
}

def {
  name = "string/pack", group = "string",
  skip = function ()
    if not string_pack then
      return "string.pack is not available in this interpreter (LuaJIT 2.1)"
    end
  end,
  run = function ()
    local n = N(200000)
    local len = 0
    for i = 1, n do
      len = len + #string_pack("<I4", i)
    end
    return len
  end,
}

-- object ------------------------------------------------------------

def {
  name = "object/closures", group = "object",
  run = function ()
    local n = N(1500000)
    local x = 0
    local f = function (v)
      x = x + v
      return x
    end
    for i = 1, n do f(i) end
    return x
  end,
}

def {
  name = "object/leaf_call", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (x) return x * 2 end
    local s = 0
    for i = 1, n do s = s + f(i) end
    return s
  end,
}

def {
  name = "object/leaf_chain", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (x) return x * 2 + 1 end
    local s = 0
    for i = 1, n do s = s + f(i) end
    return s
  end,
}

def {
  name = "object/leaf_twoarg", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (a, b) return a * b + b end
    local s = 0
    for i = 1, n do s = s + f(i, 2) end
    return s
  end,
}

def {
  name = "object/leaf_branch", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (x)
      if x > 5 then return x * 2 else return x + 1 end
    end
    local s = 0
    for i = 1, n do s = s + f(i) end
    return s
  end,
}

def {
  name = "object/leaf_threearg", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (a, b, c) return a * b + c end
    local s = 0
    for i = 1, n do s = s + f(i, 2, 3) end
    return s
  end,
}

def {
  name = "object/leaf_twoarg_branch", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (a, b)
      if a > b then return a - b else return b - a end
    end
    local s = 0
    for i = 1, n do s = s + f(i, 500000) end
    return s
  end,
}

def {
  name = "object/leaf_fourarg", group = "object",
  run = function ()
    local n = N(1500000)
    local function f (a, b, c, d) return a * b + c + d end
    local s = 0
    for i = 1, n do s = s + f(i, 2, 3, 4) end
    return s
  end,
}

def {
  name = "object/method", group = "object",
  run = function ()
    local n = N(1000000)
    method_obj.v = 0
    for i = 1, n do method_obj:add(i) end
    return method_obj.v
  end,
}

def {
  name = "object/metatable", group = "object",
  run = function ()
    local n = N(1000000)
    local s = 0
    for i = 1, n do s = s + meta_obj[i] end
    return s
  end,
}

-- coroutine ---------------------------------------------------------

def {
  name = "coroutine/ping_pong", group = "coroutine",
  run = function ()
    local n = N(125000)
    local function worker (m)
      for i = 1, m do coroutine.yield(i) end
      return m
    end
    local a = coroutine.create(worker)
    local b = coroutine.create(worker)
    coroutine.resume(a, n)
    coroutine.resume(b, n)
    local s = 0
    for i = 1, n - 1 do
      local _, va = coroutine.resume(a, i)
      local _, vb = coroutine.resume(b, i)
      s = s + va + vb
    end
    local _, fa = coroutine.resume(a, n)
    local _, fb = coroutine.resume(b, n)
    return s + fa + fb
  end,
}

-- varargs -----------------------------------------------------------

local function va_sum (...)
  local n = select("#", ...)
  local s = 0
  for i = 1, n do s = s + select(i, ...) end
  return s
end

def {
  name = "varargs/select", group = "varargs",
  run = function ()
    local n = N(80000)
    local s = 0
    for i = 1, n do
      s = s + va_sum(i, i + 1, i + 2, i + 3, i + 4, i + 5, i + 6, i + 7)
    end
    return s
  end,
}


-- ---------------------------------------------------------------- --
-- Statistics                                                       --
-- ---------------------------------------------------------------- --

local function median (arr)
  local n = #arr
  if n == 0 then return nil end
  local t = {}
  for i = 1, n do t[i] = arr[i] end
  sort(t)
  if n % 2 == 1 then
    return t[(n + 1) / 2]
  end
  return (t[n / 2] + t[n / 2 + 1]) / 2
end


local function mean (arr)
  local n = #arr
  if n == 0 then return nil end
  local s = 0
  for i = 1, n do s = s + arr[i] end
  return s / n
end


local function best (arr)
  local b = nil
  for i = 1, #arr do
    if not b or arr[i] < b then b = arr[i] end
  end
  return b
end


-- ---------------------------------------------------------------- --
-- Execution                                                        --
-- ---------------------------------------------------------------- --

local function run_bench (b, opts, baseline, baseline_label, wall_now, wall_kind)
  local rec = {
    name = b.name,
    group = b.group,
    status = "ok",
    batch = 1,
    samples_cpu = {},
  }
  if b.note then rec.note = b.note end

  if b.skip then
    local reason = b.skip()
    if reason then
      rec.status = "skipped"
      rec.note = reason
      return rec
    end
  end

  if b.prepare then
    local ok, err = pcall(b.prepare)
    if not ok then
      rec.status = "error"
      rec.error = "prepare failed: " .. tostring(err)
      return rec
    end
  end

  local run = b.run

  local ok, expected = pcall(run)
  if not ok then
    rec.status = "error"
    rec.error = "initial run failed: " .. tostring(expected)
    return rec
  end
  if expected == nil then
    rec.status = "error"
    rec.error = "benchmark returned nil"
    return rec
  end

  for i = 1, opts.warmup do
    local wok, wv = pcall(run)
    if not wok then
      rec.status = "error"
      rec.error = "warmup failed: " .. tostring(wv)
      return rec
    end
    if wv ~= expected then
      rec.status = "error"
      rec.error = "nondeterministic result during warmup"
      return rec
    end
  end

  -- Pilot: choose a batch size so one sample is ~20 ms on this VM.  The
  -- per-run time is what gets reported, so different batch sizes remain
  -- comparable across interpreters.
  local p0 = os.clock()
  local pok, pv = pcall(run)
  local p1 = os.clock()
  if not pok then
    rec.status = "error"
    rec.error = "pilot run failed: " .. tostring(pv)
    return rec
  end
  if pv ~= expected then
    rec.status = "error"
    rec.error = "nondeterministic result during pilot"
    return rec
  end
  local pilot = p1 - p0
  local batch = floor(0.020 / max(pilot, 1e-9))
  if batch < 1 then batch = 1 end
  if batch > 200 then batch = 200 end
  rec.batch = batch

  local samples = {}
  local failed = nil
  local wall_sample = nil
  for s = 1, opts.reps do
    collectgarbage("collect")
    local sink
    local w0
    if s == 1 then w0 = wall_now() end
    local c0 = os.clock()
    local sok, serr = pcall(function ()
      for j = 1, batch do
        sink = run()
      end
    end)
    local c1 = os.clock()
    if w0 then
      local w1 = wall_now()
      if w1 and w1 > w0 then wall_sample = (w1 - w0) / batch end
    end
    if not sok then
      failed = "timed run failed: " .. tostring(serr)
      break
    end
    if sink ~= expected then
      failed = "nondeterministic result in timed run"
      break
    end
    samples[#samples + 1] = (c1 - c0) / batch
  end

  if failed then
    rec.status = "error"
    rec.error = failed
    return rec
  end
  if #samples == 0 then
    rec.status = "error"
    rec.error = "no timed samples"
    return rec
  end

  rec.samples_cpu = samples
  rec.best_cpu = best(samples)
  rec.median_cpu = median(samples)
  rec.mean_cpu = mean(samples)
  rec.runs_per_sec = 1 / rec.best_cpu
  if wall_sample then
    rec.wall_seconds = wall_sample
  end

  if baseline and baseline[b.name] and baseline[b.name].best_cpu then
    rec.slowdown = rec.best_cpu / baseline[b.name].best_cpu
    rec.baseline_label = baseline_label
  elseif opts.label and opts.label == "lua" then
    rec.slowdown = 1.0
    rec.baseline_label = "lua"
  end

  return rec
end


local function load_baseline (path)
  local text, err = read_file(path)
  if not text then error("cannot read baseline " .. tostring(path) .. ": " .. tostring(err)) end
  local ok, j = pcall(json_decode, text)
  if not ok then error("cannot parse baseline " .. tostring(path) .. ": " .. tostring(j)) end
  local map = {}
  for _, r in ipairs(j.results or {}) do
    if r.status == "ok" and r.best_cpu then map[r.name] = r end
  end
  local label = (j.meta and j.meta.label) or "lua"
  return map, label
end


local function select_benches (sels)
  local out, seen = {}, {}
  local function add (b)
    if not seen[b.name] then
      seen[b.name] = true
      out[#out + 1] = b
    end
  end
  if #sels == 0 then
    for _, b in ipairs(BENCH) do add(b) end
    return out
  end
  for _, sel in ipairs(sels) do
    local matched = false
    for _, b in ipairs(BENCH) do
      if sel == "all" or b.name == sel or b.group == sel
         or b.name:sub(1, #sel) == sel .. "/" then
        add(b)
        matched = true
      end
    end
    if not matched then
      error("no benchmark matches '" .. sel .. "' (use --list)")
    end
  end
  return out
end


-- ---------------------------------------------------------------- --
-- Output                                                           --
-- ---------------------------------------------------------------- --

local function fmt_rate (r)
  if not r then return "-" end
  if r >= 1e6 then return format("%.2fM", r / 1e6) end
  if r >= 1e3 then return format("%.2fk", r / 1e3) end
  return format("%.1f", r)
end


local function fmt_ms (t)
  if not t then return "-" end
  return format("%.3f", t * 1000)
end


local function print_table (meta, results)
  io.write(format("Luar benchmark: %s -- %s\n", meta.label, meta.version))
  io.write(format("lua=%s  jit=%s%s  wall=%s  repeat=%d warmup=%d scale=%g\n",
    meta.lua_version,
    meta.jit and "on" or "off",
    (meta.jit_flags ~= "" and (" [" .. meta.jit_flags .. "]")) or "",
    meta.wall_clock, meta.reps, meta.warmup, meta.scale))
  io.write("\n")
  io.write(format("%-24s %5s %10s %10s %10s %10s %8s\n",
    "benchmark", "batch", "best ms", "median ms", "wall ms", "runs/s", "vs lua"))
  io.write(string.rep("-", 92) .. "\n")
  for _, r in ipairs(results) do
    if r.status == "ok" then
      local ratio = r.slowdown and format("%.2f", r.slowdown)
      if not ratio and not meta.baseline and meta.label == "lua" then
        ratio = "1.00"
      end
      ratio = ratio or "-"
      io.write(format("%-24s %5d %10s %10s %10s %10s %8s\n",
        r.name, r.batch, fmt_ms(r.best_cpu), fmt_ms(r.median_cpu),
        fmt_ms(r.wall_seconds), fmt_rate(r.runs_per_sec), ratio))
    else
      io.write(format("%-24s %5s %10s %10s %10s %10s %8s\n",
        r.name, "-", "-", "-", "-", "-", r.status))
    end
  end

  local notes = {}
  for _, r in ipairs(results) do
    if r.status ~= "ok" or r.note then
      local msg = r.error or r.note or r.status
      notes[#notes + 1] = format("  %-24s %s: %s", r.name, r.status, msg)
    end
  end
  if #notes > 0 then
    io.write("\nnotes:\n")
    for _, n in ipairs(notes) do io.write(n, "\n") end
  end

  if not meta.baseline and meta.label ~= "lua" then
    io.write("\n(no --baseline given; 'vs lua' is unavailable)\n")
  end
end


-- ---------------------------------------------------------------- --
-- Markdown report from JSON result files                           --
-- ---------------------------------------------------------------- --

local function merge_results (a, b)
  if not a then return b end
  if not b then return a end
  if a.status ~= "ok" and b.status == "ok" then return b end
  if b.status ~= "ok" and a.status ~= "ok" then
    if a.status == "skipped" then return a end
    return a
  end
  local r = {}
  for k, v in pairs(a) do r[k] = v end
  for k, v in pairs(b) do if r[k] == nil then r[k] = v end end
  local samples = {}
  for _, v in ipairs(a.samples_cpu or {}) do samples[#samples + 1] = v end
  for _, v in ipairs(b.samples_cpu or {}) do samples[#samples + 1] = v end
  r.samples_cpu = samples
  if #samples > 0 then
    r.best_cpu = best(samples)
    r.median_cpu = median(samples)
    r.mean_cpu = mean(samples)
    r.runs_per_sec = 1 / r.best_cpu
  end
  local wa, wb = a.wall_seconds, b.wall_seconds
  local na, nb = #(a.samples_cpu or {}), #(b.samples_cpu or {})
  if wa and wb and na + nb > 0 then
    r.wall_seconds = (wa * na + wb * nb) / (na + nb)
  else
    r.wall_seconds = wa or wb
  end
  if a.note and b.note and a.note ~= b.note then
    r.note = a.note .. "; " .. b.note
  end
  return r
end


local function report_markdown (files)
  local datasets, by_label, order = {}, {}, {}
  for _, path in ipairs(files) do
    local text, err = read_file(path)
    if not text then
      error("cannot read " .. path .. ": " .. tostring(err))
    end
    local ok, j = pcall(json_decode, text)
    if not ok then
      error("cannot parse " .. path .. ": " .. tostring(j))
    end
    local meta = j.meta or {}
    local label = meta.label or ("dataset" .. (#order + 1))
    local ds = by_label[label]
    if not ds then
      ds = {
        label = label, meta = meta, results = {},
        names = {}, name_seen = {}, files = {},
      }
      by_label[label] = ds
      order[#order + 1] = ds
    end
    ds.files[#ds.files + 1] = path
    for _, r in ipairs(j.results or {}) do
      if not ds.name_seen[r.name] then
        ds.name_seen[r.name] = true
        ds.names[#ds.names + 1] = r.name
      end
      ds.results[r.name] = merge_results(ds.results[r.name], r)
    end
  end

  if #order == 0 then error("no report inputs") end

  local names, seen = {}, {}
  for _, ds in ipairs(order) do
    for _, n in ipairs(ds.names) do
      if not seen[n] then
        seen[n] = true
        names[#names + 1] = n
      end
    end
  end

  local base = order[1]
  local out = {}

  out[#out + 1] = "## Timings"
  out[#out + 1] = ""
  out[#out + 1] = "Best (median) CPU milliseconds per run, best-of-N. " ..
    "`x` is time relative to `" .. base.label ..
    "` (lower is faster; e.g. `0.20x` means 5x faster)."
  out[#out + 1] = ""

  local head = { "benchmark" }
  local sep = { "---" }
  for i, ds in ipairs(order) do
    head[#head + 1] = ds.label .. " best (median) ms"
    sep[#sep + 1] = "---:"
    if i > 1 then
      head[#head + 1] = ds.label .. " x"
      sep[#sep + 1] = "---:"
    end
  end
  out[#out + 1] = "| " .. concat(head, " | ") .. " |"
  out[#out + 1] = "| " .. concat(sep, " | ") .. " |"

  local function cell (r)
    if not r then return "-" end
    if r.status ~= "ok" or not r.best_cpu then
      if r.status == "skipped" then return "n/a" end
      if r.status == "error" then return "error" end
      return "-"
    end
    return format("%.2f (%.2f)", r.best_cpu * 1000, (r.median_cpu or r.best_cpu) * 1000)
  end

  local function ratio_cell (r, bref)
    if not r or r.status ~= "ok" or not r.best_cpu
       or not bref or bref.status ~= "ok" or not bref.best_cpu then
      return "-"
    end
    return format("%.2fx", r.best_cpu / bref.best_cpu)
  end

  for _, name in ipairs(names) do
    local row = { name }
    local bref = base.results[name]
    for i, ds in ipairs(order) do
      local r = ds.results[name]
      row[#row + 1] = cell(r)
      if i > 1 then row[#row + 1] = ratio_cell(r, bref) end
    end
    out[#out + 1] = "| " .. concat(row, " | ") .. " |"
  end

  out[#out + 1] = ""
  out[#out + 1] = "## Throughput"
  out[#out + 1] = ""
  out[#out + 1] = "Runs per second from the best CPU sample."
  out[#out + 1] = ""

  local head2 = { "benchmark" }
  local sep2 = { "---" }
  for _, ds in ipairs(order) do
    head2[#head2 + 1] = ds.label .. " runs/s"
    sep2[#sep2 + 1] = "---:"
  end
  out[#out + 1] = "| " .. concat(head2, " | ") .. " |"
  out[#out + 1] = "| " .. concat(sep2, " | ") .. " |"
  for _, name in ipairs(names) do
    local row = { name }
    for _, ds in ipairs(order) do
      local r = ds.results[name]
      if r and r.status == "ok" and r.runs_per_sec then
        row[#row + 1] = fmt_rate(r.runs_per_sec)
      elseif r and r.status == "skipped" then
        row[#row + 1] = "n/a"
      elseif r then
        row[#row + 1] = "error"
      else
        row[#row + 1] = "-"
      end
    end
    out[#out + 1] = "| " .. concat(row, " | ") .. " |"
  end

  local notes = {}
  for _, ds in ipairs(order) do
    for _, name in ipairs(ds.names) do
      local r = ds.results[name]
      if r and (r.status ~= "ok" or r.note) then
        local msg = r.error or r.note or r.status
        notes[#notes + 1] = format("- `%s` / `%s` (%s): %s",
          ds.label, name, r.status, msg)
      end
    end
  end
  if notes then
    if #notes > 0 then
      out[#out + 1] = ""
      out[#out + 1] = "## Notes"
      out[#out + 1] = ""
      for _, n in ipairs(notes) do out[#out + 1] = n end
    end
  end

  out[#out + 1] = ""
  out[#out + 1] = "## Inputs"
  out[#out + 1] = ""
  for _, ds in ipairs(order) do
    local m = ds.meta
    out[#out + 1] = format("- `%s`: %s (%s), jit=%s, repeat=%s, wall=%s",
      ds.label, tostring(m.version), tostring(m.lua_version),
      m.jit and "on" or "off", tostring(m.reps), tostring(m.wall_clock))
  end
  return concat(out, "\n") .. "\n"
end


-- ---------------------------------------------------------------- --
-- Main                                                             --
-- ---------------------------------------------------------------- --

local function die (msg)
  io.stderr:write("bench.lua: ", msg, "\n")
  os.exit(2)
end


local function usage ()
  io.write([[
usage: bench.lua [all|GROUP|NAME ...] [options]

  --bench NAME[,NAME...]  select benchmarks (exact name, group, or all)
  --repeat N              timed samples per benchmark (default 5)
  --warmup N              warmup runs per benchmark (default 3)
  --scale F               multiply workload sizes (default 1.0)
  --json                  machine-readable output
  --baseline PATH         stock-Lua JSON result file for "vs lua"
  --label NAME            interpreter label (default: auto)
  --wall MODE             auto|date|uptime|clock
  --report FILE [...]     merge JSON files into a Markdown report
  --list                  list benchmarks
  --help                  show this help
]])
end


local function parse_args (argv)
  local opts = {
    reps = 5, warmup = 3, scale = 1.0, json = false,
    label = nil, baseline = nil, wall = "auto",
    report = false, files = {}, selections = {}, list = false,
  }
  local i = 1
  while i <= #argv do
    local a = argv[i]
    local inline
    local k, v = a:match("^(%-%-[%w%-]+)=(.*)$")
    if k then a = k; inline = v end

    local function value ()
      if inline then return inline end
      i = i + 1
      if i > #argv then die("missing argument for " .. a) end
      return argv[i]
    end

    if a == "--bench" then
      for part in value():gmatch("[^,]+") do
        opts.selections[#opts.selections + 1] = part
      end
    elseif a == "--repeat" then
      opts.reps = tonumber(value())
      if not opts.reps or opts.reps < 1 or opts.reps ~= floor(opts.reps) then
        die("--repeat needs a positive integer")
      end
    elseif a == "--warmup" then
      opts.warmup = tonumber(value())
      if not opts.warmup or opts.warmup < 0 or opts.warmup ~= floor(opts.warmup) then
        die("--warmup needs a non-negative integer")
      end
    elseif a == "--scale" then
      opts.scale = tonumber(value())
      if not opts.scale or opts.scale <= 0 then
        die("--scale needs a positive number")
      end
    elseif a == "--json" then
      opts.json = true
    elseif a == "--baseline" then
      opts.baseline = value()
    elseif a == "--label" then
      opts.label = value()
    elseif a == "--wall" then
      opts.wall = value()
    elseif a == "--report" then
      opts.report = true
      for j = i + 1, #argv do opts.files[#opts.files + 1] = argv[j] end
      i = #argv
    elseif a == "--list" then
      opts.list = true
    elseif a == "--help" or a == "-h" then
      usage()
      os.exit(0)
    elseif a:sub(1, 1) == "-" then
      die("unknown option " .. a)
    else
      for part in a:gmatch("[^,]+") do
        opts.selections[#opts.selections + 1] = part
      end
    end
    i = i + 1
  end
  return opts
end


local function main ()
  local opts = parse_args(arg or {})

  if opts.list then
    local lok, lsel = pcall(select_benches, opts.selections)
    if not lok then die(tostring(lsel)) end
    for _, b in ipairs(lsel) do
      io.write(format("%-24s %s\n", b.name, b.group))
    end
    return
  end

  if opts.report then
    if #opts.files == 0 then die("--report needs at least one JSON file") end
    local ok, md = pcall(report_markdown, opts.files)
    if not ok then die(tostring(md)) end
    io.write(md)
    return
  end

  SCALE = opts.scale

  local selected
  local ok, err = pcall(select_benches, opts.selections)
  if not ok then die(tostring(err)) end
  selected = err

  local wok, wfn, wkind = pcall(resolve_wall, opts.wall)
  if not wok then die(tostring(wfn)) end
  local wall_now, wall_kind = wfn, wkind

  local baseline, baseline_label
  if opts.baseline then
    local bok, bmap, blabel = pcall(load_baseline, opts.baseline)
    if not bok then die(tostring(bmap)) end
    baseline, baseline_label = bmap, blabel
  end

  local meta = collect_meta(opts, wall_kind)
  if baseline_label then meta.baseline_label = baseline_label end

  local results = {}
  for _, b in ipairs(selected) do
    if not opts.json then
      io.stderr:write(format("  %-24s ", b.name))
      io.stderr:flush()
    end
    local rec = run_bench(b, opts, baseline, baseline_label, wall_now, wall_kind)
    results[#results + 1] = rec
    if not opts.json then
      io.stderr:write(rec.status, "\n")
    end
  end

  if opts.json then
    io.write(json_encode({ meta = meta, results = results }), "\n")
  else
    print_table(meta, results)
  end
end


main()
