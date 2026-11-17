# LuaR FFI

The LuaR FFI is a compact, opt-in implementation of the LuaJIT `ffi`
library for the Lua 5.5 runtime.  It is built on [libffi](https://libffi.org/)
and `dlopen`/`dlsym`, and follows LuaJIT where practical.  This document
describes what is implemented, the exact semantics, and the intentional
differences.

## Building and loading

```sh
make WITH_FFI=1            # builds src/luar with the FFI
./src/luar -e 'local ffi = require "ffi"; print(ffi.sizeof("int"))'
make WITH_FFI=0            # stub: no FFI, luar.features.ffi == false
```

Requirements: libffi 3.8 or newer headers (`#include <ffi.h>`) at build
time, POSIX `dlopen`, a C99 compiler, and the libffi shared library at run
time.  LuaR does **not** link against libffi: the entry points are resolved
with `dlopen`/`dlsym` when the FFI module is opened.  Consequently
`libluar.a` has no unresolved libffi symbols and a host embedding LuaR does
not need to add `-lffi` to its own link line.  The
implementation is Linux/x86-64 tested; it uses the platform C ABI (System V
on the tested target).

`luar_openlibs()` installs the `ffi` library automatically when the build
has FFI support, and registers it in `package.preload`; both the global
`ffi` and `require("ffi")` work.  A `WITH_FFI=0` build leaves the global
`type()`/`tonumber()` functions untouched and calling `luaopen_ffi()`
directly reports that the feature is absent.

Implementation files:

| File | Contents |
| --- | --- |
| `src/luar_ffi_type.c` | C type objects, interning, layout, representations |
| `src/luar_ffi_parse.c` | lexer and recursive-descent declaration parser |
| `src/luar_ffi.c` | cdata, metamethods, `ffi.*`, finalizers |
| `src/luar_ffi_call.c` | `ffi.C`, `ffi.load`, libffi calls and callbacks |

## Module surface

```lua
local ffi = require "ffi"

ffi.cdef(s)                 -- parse C declarations
ffi.typeof(ct)              -- CType object (also accepts a cdata value)
ffi.istype(ct, obj)         -- exact C type check
ffi.sizeof(ct [, nelem])    -- size in bytes
ffi.alignof(ct)             -- alignment in bytes
ffi.offsetof(ct, field)     -- byte offset of a struct/union member
ffi.new(ct [, ...])         -- allocate a cdata value
ffi.cast(ct, value)         -- scalar/pointer/aggregate conversion
ffi.string(ptr [, len])     -- Lua string from char pointer/array
ffi.copy(dst, src [, len])  -- memcpy-like copy
ffi.fill(dst, len [, c])    -- memset-like fill
ffi.gc(cdata, finalizer)    -- attach a Lua or C finalizer
ffi.metatype(ct, metatable) -- attach a per-struct metatable
ffi.load(name [, global])   -- open a shared library namespace
ffi.errno([new])            -- read/set C errno, returns the old value
ffi.abi(name)               -- ABI predicate
ffi.os, ffi.arch            -- "Linux", "x64" on the tested target
ffi.NULL                    -- nil (LuaJIT-compatible)
ffi.C                       -- the default C namespace
```

`ffi.typeof` accepts a C type declaration (an abstract type expression such
as `"int *"`, `"int[4]"`, `"struct Foo"`, or `"int (*)(const char *, int)"`),
a CType object, or a cdata value.  CType objects are ordinary cdata objects
whose textual representation is `ctype<...>`; they are interned per Lua
state, so the same type always yields the same object (`rawequal` is true).

`ffi.sizeof` accepts a CType object, a type string, or a cdata value.  For a
variable-length array it needs the length, either as the second argument or
from the cdata object.  `ffi.alignof` and `ffi.offsetof` accept the same
kind of first argument (the latter only for struct/union types).

`ffi.abi` returns `true` for `"64bit"`, `"le"`, `"fpu"`, and `"hardfp"` on
the tested x86-64 Linux target, and `false` for unknown predicates and for
`"32bit"`, `"be"`, `"softfp"`, `"eabi"`, `"win"`, `"gc64"`, `"dualnum"`,
`"pauth"`, and `"uwp"`.

## Accepted C declarations (`ffi.cdef`)

The parser is a recursive-descent parser over the declaration subset used
by C APIs.  It supports:

* primitive types: `void`, `_Bool`/`bool`, `char`, `short`, `int`, `long`,
  `long long`, the `signed`/`unsigned` variants, `float`, `double`;
* fixed-width/common names: `int8_t` … `uint64_t`, `intptr_t`, `uintptr_t`,
  `size_t`, `ssize_t`, `ptrdiff_t`, `intmax_t`, `uintmax_t`, `time_t`;
* `typedef`, including typedefs of anonymous structs/unions and arrays;
* `struct` and `union`: named, anonymous, nested definitions, multiple
  declarators, arrays as members, anonymous struct/union members (their
  fields are found directly by `ffi.offsetof`/field access);
* `enum`: named and anonymous, explicit and implicit values, values used
  later as integer constant expressions, and enum constants in `ffi.C`;
* pointers to any supported type, fixed arrays `T[N]`, variable-length
  arrays `T[?]` (and `T[]` as an abstract type), multidimensional arrays,
  function types/prototypes with named or abstract parameters, and
  function pointers, e.g. `int (*cb)(const char *, void *)`;
* qualifiers `const`, `volatile`, `restrict`, `__restrict`, GNU spellings
  such as `__const`, and storage specifiers (`typedef`, `extern`, `static`,
  `register`, `auto`, `inline`); qualifiers are accepted and ignored;
* `/* ... */` and `//` comments and semicolon-separated declarations;
* array sizes and enum values built from integer literals, other enum
  constants, parentheses, unary `+ - ~ !`, and binary `+ - * / % << >> & |
  ^`.

`ffi.cdef` may be called with several strings.  Declarations must be
complete: function definitions are not accepted, and only the documented
preprocessor subset is available.

### Parser limitations (clear errors, never silent misparses)

* A small C preprocessor is implemented: object- and function-like
  `#define`/`#undef`, `#ifdef`/`#ifndef`/`#if`/`#else`/`#endif`, with
  object macros expanded outside strings/comments.  Function-like macros
  support named and variadic (`...`/`__VA_ARGS__`) parameters on a single
  line.  `#`/`##`, `#include`, non-pack `#pragma` and complex `#if`
  expressions are rejected; `#pragma pack(n/push/pop)` drives layout.
* `__attribute__((packed))` and `__attribute__((aligned(n)))` (alone or
  combined) on a struct/union are supported; other attributes,
  `__declspec`, `_Atomic`, `_Complex`, `__int128`, inline asm and
  `_Static_assert` are rejected.
* `long double` is rejected.  Only 4-byte `float` and 8-byte `double` are
  supported.
* `__attribute__((packed))`/`aligned(n)` and `#pragma pack(n)` (including
  `push`/`pop`) control layout and are field-accessible; packed aggregates
  cannot be passed by value (libffi would mislay them); layout
  always follows the natural C ABI of the target.
* Zero-length arrays are rejected; use `[?]`/`[]` for a variable-length
  type and pass the length to `ffi.new`/`ffi.sizeof`.
* A bare declaration of a named struct/union inside another struct only
  declares the tag; it does not create an anonymous member.  Tagless
  `struct { ... };` members do create anonymous members.
* Typedefs are resolved to their underlying type for identity and
  representation.  For example, `int64_t` prints as the underlying
  `long`/`long long` on the platform (`ctype<long>`), and
  `ffi.istype("size_t", ffi.new("unsigned long"))` is true.  This is a
  representation deviation from LuaJIT, not an ABI deviation.

Error messages carry the source name, line number, the reason, and the
offending token (for example
`ffi.cdef:1: expected ';' near 'foo'`).

## C type model and layout

Sizes and alignments are those of the host C compiler.  On x86-64 Linux:

| Type | Size | Align |
| --- | --- | --- |
| `char`, `_Bool` | 1 | 1 |
| `short` | 2 | 2 |
| `int`, `float`, enum | 4 | 4 |
| `long`, `long long`, `double`, pointer | 8 | 8 |

Struct layout uses natural C rules: each member is aligned to its own
alignment, the struct alignment is the maximum member alignment, and the
total size is rounded up to that alignment.  Unions overlay all members at
offset 0 and use the largest member size/alignment.  Anonymous members do
not change offsets; their members are found by recursive lookup.

Incomplete types (`struct Foo;`) are allowed, can be used behind pointers,
and are completed by a later `struct Foo { ... };` definition.  Sizes and
offsets of an incomplete type are errors until it is completed.
Struct/union definitions cannot be redefined in the same Lua state.

## cdata values

`ffi.new(ct, ...)` creates a cdata value:

* Scalars store their value in the cdata object; a missing initializer is
  zero.  `ffi.new("int")` is a `cdata<int>` zero.
* Arrays allocate their storage inline in the cdata object.  Fixed arrays
  use their declared length; `T[?]` requires the length as the first
  argument after the type.  Arrays can be initialized with a Lua table
  (0-based or 1-based numeric keys), with positional constructor arguments
  (`ffi.new("int[3]", 1, 2, 3)`), or with a string for 1-byte element
  types.  A single initializer is replicated across the whole array, as in
  LuaJIT.  All remaining bytes are zero.
* Structs and unions are zero-initialized and can be initialized from a
  table with named and/or positional fields, or from positional arguments
  (`ffi.new("struct P", 1, 2)` fills the first two named fields; omitted
  fields are zero).  Nested aggregate members can be initialized with
  nested tables.  Unions initialize their first member.  Anonymous members
  participate in positional initialization; named table fields work for
  the anonymous member's own fields.
* `ffi.new` of an incomplete type, `void`, or a function type is an error.
* The same type is accepted in place of the type string (`ffi.new(Pos, 1,
  2)`), and a cdata of an identical aggregate type can be copied as the
  initializer.

Storage is allocated in Lua full userdata with at least `max_align_t`
alignment and an extra 16-byte-aligned cdata header.  The cdata metatable
has a `__gc` metamethod, so `ffi.gc` works on every cdata.  References to
struct/union/array fields and array elements are non-owning cdata objects
that keep their owner alive through a user value; they remain valid as long
as the owner is reachable.

### Field and array access

`cdata.field` retrieves a struct/union field (including through a pointer
to a struct, like C's `->`).  Scalar fields are converted to Lua values
(numbers, booleans, or 64-bit cdata); aggregate fields yield a reference
cdata.  `cdata.field = value` writes a field, converting with the C rules.
`ptr[i]` indexes a pointer or array; aggregate elements yield a reference,
scalars a value.  Owned arrays and variable-length arrays check bounds and
raise `ffi: array index N out of bounds`; pointer indexing cannot be
bounds-checked and is the caller's responsibility.

Out-of-bounds writes on owned arrays, `ffi.string`, `ffi.copy`, and
`ffi.fill` are rejected, including for fixed-size character arrays.  This
is intentionally stricter (safer) than LuaJIT for array-backed objects.

### scalars, pointers, int64 and comparison semantics

C conversions follow C truncation/width rules: `ffi.new("uint8_t", 300)`
stores 44, `ffi.new("int", 3.9)` stores 3, and `ffi.cast("uint8_t", -1)`
is 255.  Integer cdata objects returned by `ffi.new`/`ffi.cast` are cdata,
not Lua numbers.

Integer arithmetic on cdata follows LuaJIT's 64-bit promotion: the result
is `int64_t` cdata unless one operand is an unsigned 64-bit cdata, in which
case it is `uint64_t` cdata.  `+ - * / %` and unary `-` wrap in 64 bits;
division/modulo by zero and `INT64_MIN / -1` follow LuaJIT's sentinel
behavior.  Comparisons use the same promotion rules.

Bitwise and shift operators (`&`, `|`, `~`, `<<`, `>>`, unary `~`) and
`//`/`^` are supported with LuaJIT semantics: 64-bit two's-complement
results, `int64_t` unless an unsigned 64-bit operand applies (`& | ~ ^`
promote if either operand is unsigned; shifts follow the left operand),
shift counts masked to six bits, logical right shifts, integer powers
(with `x^-n` following LuaJIT's 0/±1 rules), and C-truncating `//`.
Pointer operands are rejected rather than reinterpreted.

`tonumber(cdata)` is supported for numeric cdata through a wrapper around
the global `tonumber` (the original function is saved and used for all
non-cdata values).  `tostring` produces LuaJIT-style output: `ctype<int>`
for CType objects, `cdata<int *>: 0x...`/`NULL` for pointers,
`cdata<struct Foo>: 0x...` for aggregates, `42LL`/`42ULL` for 64-bit
integers, and `cdata<enum E>: 42` for enums.  A metatype `__tostring` is
used for its struct type.

`ffi.NULL` is `nil`, matching LuaJIT.  In addition, any NULL pointer or
function-pointer cdata compares equal to `nil`:
`ffi.cast("int *", 0) == nil` is true.  Zero scalars, zero floats, and
arrays do not compare equal to `nil`.  Ordering comparisons with `nil`
remain errors, as in LuaJIT.  cdata numbers can also be compared directly
with Lua numbers (`ffi.new("int64_t", 5) == 5`, `< 6`, etc.); the Lua
number is converted with the same 64-bit rules, including LuaJIT's
truncation of a fractional value in this context.

### strings and memory helpers

`ffi.string(ptr [, len])` accepts a `char *`/`void *` pointer or a 1-byte
element array.  Without `len` it reads up to a NUL, bounded by the known
array size when the object is an owned array.  For pointers the caller is
responsible for NUL termination.  With `len`, the length is checked
against known array bounds.

`ffi.copy(dst, src [, len])` accepts pointer/array cdata for both sides,
and a Lua string as `src`.  Omitting `len` when `src` is a Lua string
copies `#src + 1` bytes (including the NUL), matching LuaJIT; an explicit
length of `#src + 1` is also accepted.  Copies use `memmove` semantics.
`ffi.fill(dst, len [, c])` fills `len` bytes with the low 8 bits of `c`.

Converted Lua strings are not anchored by the cdata.  When a string is
cast/assigned to a `char *`, keep the Lua string alive for as long as the
pointer is used.

### finalizers and metatypes

`ffi.gc(cdata, finalizer)` attaches a finalizer and returns the cdata.
The finalizer may be a Lua function or a C function cdata (for example
`ffi.gc(ffi.C.malloc(n), ffi.C.free)`); it is called with the cdata as its
only argument and errors from Lua finalizers are ignored.  Passing `nil`
removes the finalizer.  A C finalizer is invoked through the normal FFI
call path; it must be a fixed-argument function cdata.

`ffi.metatype(ct, metatable)` applies a metatable to one struct/union type
and returns the type object.  A type can only have one metatype; trying to
replace it raises "cannot change a protected metatable".  The supported
metatype metamethods are:

* `__index` (table or function): fallback for missing fields;
* `__newindex` (table or function): fallback for missing-field writes;
* `__tostring`: used by `tostring`;
* `__call`: makes a struct cdata callable.

Real struct fields always take priority over `__index`/`__newindex`.
Other metatype metamethods (`__eq`, `__lt`, `__len`, arithmetic, `__gc`,
etc.) are not dispatched.  Use `ffi.gc` for finalization.

## Calling C

`ffi.C` is the default namespace (`dlsym` with `RTLD_DEFAULT`).
`ffi.load(name [, global])` opens a shared library; `ffi.load()` returns
the default namespace.  On Linux, names without a slash are tried as given
for dotted names and as `lib<name>.so` otherwise, including GNU ld script
resolution (so `ffi.load("c")` resolves libc correctly).  Loaded namespaces
keep their `dlopen` handle open until the namespace and all cdata symbols
derived from it are collected; the default namespace is never closed.

**Declarations are required.**  A symbol must first be declared with
`ffi.cdef`; otherwise `ffi.C.foo` raises "missing declaration for symbol
'foo'".  This matches LuaJIT's declared-symbol model.  Enum constants
declared in `ffi.cdef` are accessible as `ffi.C.NAME` (Lua integers).
Function symbols return function cdata; data symbols return a value or a
reference to the symbol's storage.

Calls support:

* any fixed number of arguments with integer types (including
  `int64_t`/`uint64_t`), `_Bool`/`bool`, `float`, `double`, pointers,
  enums, function pointers, and arrays (decayed to a pointer);
* Lua strings for `char *`/`void *` parameters and struct fields;
* structs and unions passed by pointer, including the LuaJIT convenience of
  passing an aggregate cdata where a pointer to the exact same aggregate
  type is expected;
* unions up to 16 bytes of any size (scalars, arrays and nested structs
  included) and structs containing them by value; the union is classified
  per eightbyte (SSE/INTEGER, with mixed members merging to INTEGER) and
  mapped to libffi double/integer fields, using alignment-sized fields for
  odd sizes.  Larger unions use the memory ABI when their size is a
  multiple of a 1/2/4/8-byte alignment, with offsets/size validated for
  struct members;
* struct return/argument by value when the layout is representable by
  libffi and verified byte-for-byte against libffi's computed offsets.
  Fixed array members (including arrays of structs), all unions up to 16
  bytes, larger memory-class unions and bitfield structs are supported;
  oversized unions and arrays themselves are rejected by value;
* cdata function pointers (`ffi.C.fn`, `ffi.cast("int (*)(int)", ...)`,
  function-pointer fields/returns) are callable with the normal argument
  conversion rules;
* variadic prototypes (`...`): the declared fixed parameters are converted
  as above and each remaining argument is passed with the default
  conversions described next.  `ffi_prep_cif_var` is used so the platform
  variadic ABI (including the x86-64 `AL` register count) is correct.

Integer results up to 32 bits and enums are returned as Lua integers;
`int64_t`/`uint64_t` results are cdata; `float`/`double` are Lua numbers;
pointers and aggregates are cdata; `_Bool` returns a Lua boolean; `void`
returns no values.  `ffi.errno` is preserved across the libffi call
machinery, so the callee's errno can be read afterwards.

### Variadic argument conversions

A variadic call requires at least the fixed parameters of the prototype
(calling it with fewer is an error), and a prototype with no fixed
parameter before `...` is rejected (ISO C does not permit it, and libffi
requires one fixed parameter).  The fixed part uses the declared parameter
types, including structs by value, enums, `bool` and 64-bit integers.
Each argument in the `...` part is converted independently:

| Lua value / cdata | Passed to the callee as |
| --- | --- |
| Lua integer that fits in `int` | `int` |
| any other Lua integer | `long long` |
| Lua float | `double` |
| Lua string | `const char *` (anchored until the call returns) |
| Lua boolean | `int` (0 or 1) |
| `nil` | `void *` NULL |
| light userdata | `void *` |
| cdata integer, 4 or 8 bytes | declared signed/unsigned C type |
| cdata integer smaller than `int` | `int` (C default promotion) |
| cdata enum | `int` |
| cdata `bool` | `int` (0 or 1) |
| cdata `float` | `double` (C default promotion) |
| cdata `double` | `double` |
| cdata pointer/function | pointer of its declared type |
| cdata array | pointer to its first element |
| cdata struct/union | error: aggregate varargs are not portable |
| any other value | error: cannot pass as a variadic argument |

The C default argument promotions are applied where the ABI requires them
(small integers and `float`); libffi additionally rejects unpromoted
variadic types.  Use `ffi.new`/`ffi.cast` to force an exact integer width.
The variadic part is not type-checked against the callee's `va_arg` or
`printf` expectations, so a mismatch is undefined behavior, as in C.

Call features:

* C function pointers (including pointers returned by other C functions)
  can be passed and called normally.
* `ffi.callback(ctype, lua_function)` creates a callable cdata function
  pointer backed by a libffi closure.  The function type must be
  non-variadic.  A callback may return a struct or a small scalar union by
  value when the Lua function returns a cdata of the same layout; array
  and large aggregate returns are rejected.  Arguments are converted
  with the normal read rules; pointer/aggregate arguments are references
  valid for the duration of the callback (do not retain them after it
  returns).  If the Lua function raises, the C caller receives a zero
  return and the message is available from `ffi.callback_error()` (a LuaR
  extension that clears the stored message).  Callbacks are freed by the GC;
  a C library that keeps a pointer after the cdata is collected has the
  usual use-after-free caveat.  Callbacks must be invoked from the Lua
  state that created them.
* **Oversized unions** (>4096 alignment-sized fields); their ABI cannot
  be represented safely by the current libffi mapping.
* **C global variables** are best-effort: ordinary and `__thread`/
  `_Thread_local` data symbols can be read/written through `ffi.C` for the
  current thread; complex relocations and cross-thread access are not
  handled.

## Intentional deviations from LuaJIT

* Lua 5.5 has no native cdata type.  cdata is implemented as full userdata;
  when the FFI module loads, the global `type()` is wrapped (the original
  is saved) so that `type(cdata) == "cdata"` and `type(ctype) == "cdata"`.
  `tonumber()` is wrapped similarly.  Other userdata and all non-FFI values
  keep stock Lua 5.5 behavior.  If an application replaces `type` or
  `tonumber` *after* loading the FFI, that replacement wins.
* The Lua VM has two small `/* LUAR */` patches in `src/lvm.c` to implement
  LuaJIT's cdata↔number/NULL equality and the compiler's `EQI`/`EQK`
  equality fast paths.  They are enabled by the `LUAR_FFI_HOOKS` build
  flag, which `src/Makefile` defines in both FFI and non-FFI builds, and
  are gated by a reserved integer marker in the cdata metatable.  With no
  FFI module loaded the marker never exists, so the hooks are inert and
  ordinary userdata semantics are unchanged.  Without these patches Lua
  5.5 would never call `__eq` for `userdata == number`/`nil`, making the
  required LuaJIT behavior impossible.  Comparison ordering is handled by
  the normal `__lt`/`__le` metamethods.
* `string.format` is wrapped when the FFI loads: integer cdata formats with
  `%d %i %u %o %x %X`, `%s`/`%q` use cdata `tostring`, and float cdata is
  rejected by numeric conversions, matching LuaJIT.  Non-cdata values keep
  stock Lua 5.5 behavior.
* Typedef names are not preserved in type representations; see above.
* `ffi.string`, `ffi.copy`, and `ffi.fill` perform stricter bounds checks
  for owned arrays than LuaJIT.
* Only the metatype metamethods listed above are dispatched.
* Variadic default conversions differ from LuaJIT in two places.  LuaJIT
  passes every Lua number as `double`, while LuaR passes an integer Lua
  number as `int`/`long long` (so `printf("%d", 42)` works) and only a
  non-integer Lua number as `double`.  LuaJIT also passes struct/union
  cdata as pointers, while LuaR rejects aggregate varargs with a clear
  error because passing them by value is not portable.  Use
  `ffi.new`/`ffi.cast` to force a particular C type.
* `__thread`/`_Thread_local` globals resolve through `dlsym` for the
  calling thread; LuaR does not create or migrate threads, and another C
  thread sees its own instance.
* Bitfields are supported for integer, enum and `_Bool` fields in structs
  with GCC/x86-64 bit-granular packing, including mixed base types and
  fields crossing byte boundaries.  Unnamed padding bitfields and unnamed
  zero-width alignment fields are supported (the zero-width type affects
  the next offset but not the struct alignment, as in GCC).  Union
  bitfields share offset zero and can be read, written and passed by
  value.  Struct bitfield storage runs are grouped into synthetic integer
  fields for by-value passing, so bitfield structs work too.  `long double`, complex numbers, vectors and packing
  attributes are not implemented.  `ffi.callback`
  supports non-variadic types, including struct and scalar-union by-value
  returns when the callback returns a matching cdata; array/large aggregate
  returns fail clearly.  None are silently emulated.
* The implementation is little-endian-load based and has only been tested
  on Linux/x86-64.  It reports ABI facts (`ffi.os`/`ffi.arch`/`ffi.abi`)
  for the host it is built on; other targets need their own testing.

## Tests

The suite lives in `tests/ffi/` and is run by the normal test runner:

```sh
make clean && make WITH_FFI=1
./src/luar -e 'local ffi=require"ffi"; print(ffi.sizeof("int"))'
./src/luar tests/run.lua ffi
make WITH_FFI=0 && make test
```

The tests are deterministic and use only libc plus a tiny C helper
(`tests/ffi/helper.c`) that is compiled at test time into the gitignored
`tests/tmp/` directory.  The helper supplies layout probes, struct-by-value
entry points, a qsort wrapper with a C comparator, enum/bool/64-bit call
probes, and variadic probes.  Coverage includes:

* `ffi.cdef` success and error cases, including offending-token messages;
* object- and function-like (including variadic) cdef macros,
  conditionals, persistence, `#undef`, nested argument lists and rejection
  of stringifying/include/complex directives;
* sizes, alignments, and offsets compared against the C compiler's
  `sizeof`/`offsetof`;
* `ffi.new` scalars, arrays (fixed and VLA), structs, unions, anonymous
  members, tables/strings/positional initializers, and zeroing;
* field/array access, aggregate references and bounds checks;
* pointer casts/arithmetic/comparisons, int64/uint64 wrapping and
  comparisons, `tonumber`, and `tostring` formats;
* `ffi.string`, `ffi.copy` (including the 2-argument string form),
  `ffi.fill`, and their bounds errors;
* bitfield layout (mixed bases, byte-crossing fields, union bitfields,
  packed structs,
  signed extraction,
  masking),
  get/set, positional/named initializers and by-value rejection;
* `ffi.callback` (qsort via a real C comparator, struct-by-value arguments,
  direct calls, errors, GC lifetime) and `ffi.callback_error`;
* `ffi.metatype` constructors, `__index`/`__newindex`/`__tostring`/`__call`,
  protected metatables, and `ffi.gc` with Lua and C finalizers;
* libc calls (`strlen`, `strcmp`, `memset`, `memcpy`, `abs`, `labs`,
  `malloc`/`free`, `strerror`, `getpid`, `chdir`, `div`) and `errno`;
* `ffi.load` (including `"c"`), `ffi.C`, missing declarations, wrong
  arity, and callback rejection;
* struct-by-value arguments and returns, nested structs, fixed array
  members, small scalar unions and structs containing them, pointer fields,
  qsort via a C comparator, and enum/bool/64-bit call paths;
* variadic calls through libc (`snprintf`, `printf`): mixed fixed and
  variadic arguments, integer widths, floats, runtime-built strings,
  booleans, `nil`, cdata scalars/pointers/arrays, fixed struct/enum/bool/
  64-bit arguments, aggregate rejection, two consecutive calls, and
  `ffi.C`/`ffi.load` parity.

## Known crash/edge cases

Like every FFI, arbitrary pointer arithmetic, integer-to-pointer casts,
and passing NULL to a C function that does not accept it are unchecked and
can crash the process.  Such operations are the caller's responsibility.
Examples to avoid: indexing `ffi.cast("int *", 0)`, calling a C function
that dereferences a NULL pointer, and freeing memory through `ffi.C.free`
while another cdata still points into it.  Variadic arguments are likewise
unchecked: passing a value whose promoted type does not match the callee's
`va_arg` type or format specifier is undefined behavior.
