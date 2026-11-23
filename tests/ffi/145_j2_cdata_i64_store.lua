-- ffi/145_j2_cdata_i64_store.lua -- J2 stores into 8-byte integer cdata
-- destinations.
--
-- A 64-bit element read would produce cdata rather than a Lua number, so
-- copy/source/update forms still require a 1/2/4-byte or float source.  The
-- destination, however, is only written: the helper applies the FFI C-store
-- conversion and never loads it into a Lua slot.  This covers int64_t and
-- uint64_t copy, source-scalar and two-source stores.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/145_j2_cdata_i64_store: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/145_j2_cdata_i64_store: skipped (no native target)")
  return
end

local N = 257

local function bytes (a, ct)
  return ffi.string(ffi.cast("uint8_t *", a), N * ffi.sizeof(ct))
end

local function make (ct, fill)
  local a = ffi.new(ct .. "[?]", N)
  for i = 0, N - 1 do a[i] = fill(i) end
  return a
end

local function run (name, dct, bct, cct, f, fillb, fillc)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  local d = make(dct, function () return 0 end)
  local b = make(bct, fillb)
  local c = make(cct, fillc)
  local want = { pcall(f, d, b, c, N) }
  local want_bytes = bytes(d, dct)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local d2 = make(dct, function () return 0 end)
  local b2 = make(bct, fillb)
  local c2 = make(cct, fillc)
  local before = jit.stats().tint_hits
  local got = { pcall(f, d2, b2, c2, N) }
  local hits = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.eq(tostring(got[i]), tostring(want[i]), name .. ": result " .. i)
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  H.eq(bytes(d2, dct), want_bytes, name .. ": destination bytes")
  H.check(hits > 0, name .. ": did not use a cdata intrinsic")
end

local kinds = { "int64_t", "uint64_t" }
local srcs = { "int8_t", "uint16_t", "int32_t", "float", "double" }

local function fill_src (ct)
  return function (i)
    local v = ((i * 37) % 101) - 50
    if ct == "double" then
      if i % 29 == 0 then return 1e19 end
      if i % 31 == 0 then return -1e19 end
      return v / 3
    elseif ct == "float" then
      return v / 5
    end
    return v
  end
end

for _, dct in ipairs(kinds) do
  for _, sct in ipairs(srcs) do
    local fill = fill_src(sct)
    run("copy " .. dct .. " <- " .. sct, dct, sct, sct,
        function (d, b, c, n)
          for i = 0, n - 1 do d[i] = b[i] end
          return d[0], d[N // 2], d[N - 1]
        end, fill, fill)
    run("scalar add " .. dct .. " <- " .. sct, dct, sct, sct,
        function (d, b, c, n)
          for i = 0, n - 1 do d[i] = b[i] + 3.5 end
          return d[0], d[N // 2], d[N - 1]
        end, fill, fill)
  end
  run("pair add " .. dct, dct, "int16_t", "uint32_t",
      function (d, b, c, n)
        for i = 0, n - 1 do d[i] = b[i] + c[i] end
        return d[0], d[N // 2], d[N - 1]
      end,
      function (i) return (i % 41) - 20 end,
      function (i) return i * 7 end)
  run("pair mul " .. dct, dct, "int8_t", "double",
      function (d, b, c, n)
        for i = 0, n - 1 do d[i] = b[i] * c[i] end
        return d[0], d[N // 2], d[N - 1]
      end,
      function (i) return (i % 19) - 9 end,
      function (i) return (i % 13) / 4 end)
  run("strided " .. dct, dct, "uint16_t", "uint16_t",
      function (d, b, c, n)
        for i = 0, n - 1, 3 do d[i] = b[i] + c[i] end
        return d[0], d[1], d[N - 2], d[N - 1]
      end,
      function (i) return i % 97 end,
      function (i) return (i * 3) % 89 end)
  run("descending " .. dct, dct, "int32_t", "int32_t",
      function (d, b, c, n)
        for i = n - 1, 0, -2 do d[i] = b[i] - c[i] end
        return d[0], d[1], d[N - 2], d[N - 1]
      end,
      function (i) return i * 5 - 300 end,
      function (i) return (i % 31) - 15 end)
end

-- The in-place update form still needs to read its 64-bit destination as a
-- cdata value, so it must stay interpreted (and remain correct).
do
  local a = make("int64_t", function (i) return i * 3 - 100 end)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.off()
  for i = 0, N - 1 do a[i] = a[i] + 1 end
  local want = bytes(a, "int64_t")
  local b = make("int64_t", function (i) return i * 3 - 100 end)
  jit.flush()
  jit.opt.start("ir=0", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  for i = 0, N - 1 do b[i] = b[i] + 1 end
  local hits = jit.stats().tint_hits - before
  H.eq(hits, 0, "int64 in-place update should stay interpreted")
  H.eq(bytes(b, "int64_t"), want, "int64 update bytes")
end
