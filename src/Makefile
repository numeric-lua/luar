# Makefile for building Luar (luar, luarc, libluar.a)
# Derived from the Lua 5.5.1 build system; see ../doc/roadmap.md
# for the upstream-tracking policy.
#
# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

CC= gcc
STD= -std=gnu99
CFLAGS?= -O3 -g -Wall -Wextra
CFLAGS+= -MMD
LDFLAGS?=
LIBS= -lm $(MYLIBS) $(SYSLIBS)
PICFLAGS= -fPIC
AR= ar
ARFLAGS= rcu
RANLIB= ranlib
RM= rm -f
UNAME_S:= $(shell uname -s)

# Luar extensions are always compiled in.  FFI is opt-in until the
# Luar FFI library is complete (see doc/roadmap.md, milestone 0.2).
WITH_FFI?= 0

# J1 trace recorder.  JIT=0 compiles the backedge hooks out entirely.
WITH_JIT?= 1

ifeq ($(UNAME_S),Linux)
SYSCFLAGS= -DLUA_USE_LINUX
SYSLIBS= -Wl,-E -ldl
endif
ifeq ($(UNAME_S),Darwin)
SYSCFLAGS= -DLUA_USE_MACOSX
SYSLIBS=
endif
ifeq ($(UNAME_S),FreeBSD)
SYSCFLAGS= -DLUA_USE_POSIX -DLUA_USE_DLOPEN
SYSLIBS= -Wl,-E
endif

ifdef PREFIX
SYSCFLAGS+= -DLUA_ROOT='"$(PREFIX)/"'
endif

MYCFLAGS=
MYLDFLAGS=
MYLIBS=

FFI_O= luar_ffi_stub.o
# Objects of both FFI variants, so 'make clean' also removes a previous
# WITH_FFI=1 build.
ALL_FFI_O= luar_ffi_stub.o luar_ffi.o luar_ffi_type.o luar_ffi_parse.o \
	luar_ffi_call.o luar_ffi_preproc.o luar_ffi_trace.o
ifeq ($(WITH_JIT),0)
SYSCFLAGS+= -DLUAR_JIT_DISABLE
endif

ifeq ($(WITH_FFI),1)
# The FFI is split into a type system, a C declaration parser, the cdata
# runtime/library and the libffi call/C-library layer.  libffi itself is
# loaded with dlopen()/dlsym() when the module opens (so luar binaries and
# embedders have no link-time dependency on it; the headers are still needed
# to build).
FFI_O= luar_ffi.o luar_ffi_type.o luar_ffi_parse.o luar_ffi_call.o \
	luar_ffi_preproc.o luar_ffi_trace.o
endif
# The VM has two small inert hook points for FFI cdata equality (see
# doc/ffi.md).  Compile them in both variants so switching WITH_FFI does
# not require a clean rebuild of the whole core.
SYSCFLAGS+= -DLUAR_FFI_HOOKS

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

LUA_A=	libluar.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
	lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o \
	lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o \
	loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
LUAR_O=	luar_lib.o luar_jit.o luar_emit.o luar_trace.o $(FFI_O)
BASE_O= $(CORE_O) $(LIB_O) $(LUAR_O)

LUA_T=	luar
LUA_O=	lua.o

LUAC_T=	luarc
LUAC_O=	luac.o

ALL_O= $(BASE_O) $(LUA_O) $(LUAC_O)
ALL_T= $(LUA_A) $(LUA_T) $(LUAC_T)

# Shared library.  The three names follow the usual libtool convention:
#   libluar.so.<version>   real file
#   libluar.so.<major>     SONAME (what executables record)
#   libluar.so             unversioned link-time name
# VERSION is passed by the top-level Makefile; the default keeps 'make -C src'
# usable on its own.  PIC objects are separate from the static ones.
VERSION?= 0.235.0
SOVERSION= $(firstword $(subst ., ,$(VERSION)))
UNAME_S:= $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LUA_SO= libluar.$(VERSION).dylib
LUA_SONAME= libluar.$(SOVERSION).dylib
LUA_SOLINK= libluar.dylib
else
LUA_SO= libluar.so.$(VERSION)
LUA_SONAME= libluar.so.$(SOVERSION)
LUA_SOLINK= libluar.so
endif
# Export list for the ELF linker (see etc/libluar.map).  Kept outside src/
# so tools/upstream-check.sh does not classify it as an upstream addition.
LUA_MAP= ../etc/libluar.map
PIC_O= $(BASE_O:.o=.pic.o)

# Linking a shared library must not use -Wl,-E (that is for executables that
# export their symbols to C modules), and on macOS the flag names differ.
SHARED_LDFLAGS= -shared -Wl,-soname,$(LUA_SONAME)
SHARED_MAP= -Wl,--version-script=$(LUA_MAP)
SHARED_LIBS= -lm $(MYLIBS)
ifeq ($(UNAME_S),Linux)
SHARED_LIBS+= -ldl
endif
ifeq ($(UNAME_S),Darwin)
SHARED_LDFLAGS= -dynamiclib -Wl,-install_name,@rpath/$(LUA_SONAME) \
	-Wl,-compatibility_version,$(SOVERSION) -Wl,-current_version,$(VERSION)
SHARED_MAP=
SYSLIBS= -lreadline
endif

# Targets start here.
default: all

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(LUA_A)

$(LUA_A): $(BASE_O)
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(BASE_O)
	$(RANLIB) $@

$(LUA_T): $(LUA_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(MYLDFLAGS) $(LUA_O) $(LUA_A) $(LIBS)

$(LUAC_T): $(LUAC_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(MYLDFLAGS) $(LUAC_O) $(LUA_A) $(LIBS)

# -- shared library ---------------------------------------------------------
# The export map is optional in the sense that a failed/missing script is
# reported by the linker rather than silently ignored; the target depends on
# it so a change to the exported set relinks the library.
$(LUA_SO): $(PIC_O) $(LUA_MAP)
	$(CC) $(SHARED_LDFLAGS) -o $@ $(LDFLAGS) $(MYLDFLAGS) \
		$(PIC_O) $(SHARED_MAP) $(SHARED_LIBS)

# The symlinks are recreated on every 'make shared'; only the library itself
# is rebuilt when its objects change.
shared: $(LUA_SO)
	ln -sfn $(LUA_SO) $(LUA_SONAME)
	ln -sfn $(LUA_SONAME) $(LUA_SOLINK)


# DynASM's action buffer performs deliberately unaligned uint stores; keep
# that vendored code out of the alignment sanitizer so UBSan can gate the
# rest of the runtime.
luar_emit.o: luar_emit.c luar_emit_x64.h luar_emit.h
	$(CC) $(STD) $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS) -I../dynasm \
	  -fno-sanitize=alignment -c luar_emit.c

%.o: %.c
	$(CC) $(STD) $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS) -c $<

# PIC variants, used only by the shared library.  The static build keeps
# using the plain .o files above.
luar_emit.pic.o: luar_emit.c luar_emit_x64.h luar_emit.h
	$(CC) $(STD) $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS) $(PICFLAGS) -I../dynasm \
	  -fno-sanitize=alignment -c -o $@ luar_emit.c

%.pic.o: %.c
	$(CC) $(STD) $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS) $(PICFLAGS) -c -o $@ $<

test:
	./$(LUA_T) -v

clean:
	$(RM) $(ALL_T) $(ALL_O) $(ALL_FFI_O) $(ALL_O:.o=.d) $(ALL_FFI_O:.o=.d)
	$(RM) $(PIC_O) $(ALL_FFI_O:.o=.pic.o) $(PIC_O:.o=.d) $(ALL_FFI_O:.o=.pic.d)
	$(RM) $(LUA_SO) $(LUA_SONAME) $(LUA_SOLINK)

echo:
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "WITH_FFI= $(WITH_FFI)"
	@echo "WITH_JIT= $(WITH_JIT)"
	@echo "SHARED_LIB= $(LUA_SO) (SONAME $(LUA_SONAME))"

.PHONY: all default o a shared test clean echo

# Header dependencies generated by -MMD.
-include $(ALL_O:.o=.d) $(PIC_O:.o=.d) $(ALL_FFI_O:.o=.d)
