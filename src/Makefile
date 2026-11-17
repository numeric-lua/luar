# Makefile for building LuaR (luar, luarc, libluar.a)
# Derived from the Lua 5.5.1 build system; see ../doc/roadmap.md
# for the upstream-tracking policy.
#
# == CHANGE THE SETTINGS BELOW TO SUIT YOUR ENVIRONMENT =======================

CC= gcc
STD= -std=gnu99
CFLAGS?= -O2 -g -Wall -Wextra
LDFLAGS?=
LIBS= -lm $(MYLIBS) $(SYSLIBS)
AR= ar
ARFLAGS= rcu
RANLIB= ranlib
RM= rm -f
UNAME_S:= $(shell uname -s)

# LuaR extensions are always compiled in.  FFI is opt-in until the
# LuaR FFI library is complete (see doc/roadmap.md, milestone 0.2).
WITH_FFI?= 0

ifeq ($(UNAME_S),Linux)
SYSCFLAGS= -DLUA_USE_LINUX
SYSLIBS= -Wl,-E -ldl
endif
ifeq ($(UNAME_S),Darwin)
SYSCFLAGS= -DLUA_USE_MACOSX
SYSLIBS= -Wl,-E
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
ifeq ($(WITH_FFI),1)
FFI_O= luar_ffi.o
LIBS+= -lffi
SYSCFLAGS+= -DLUAR_HAS_FFI
endif

# == END OF USER SETTINGS -- NO NEED TO CHANGE ANYTHING BELOW THIS LINE =======

LUA_A=	libluar.a
CORE_O=	lapi.o lcode.o lctype.o ldebug.o ldo.o ldump.o lfunc.o lgc.o llex.o \
	lmem.o lobject.o lopcodes.o lparser.o lstate.o lstring.o ltable.o ltm.o \
	lundump.o lvm.o lzio.o
LIB_O=	lauxlib.o lbaselib.o lcorolib.o ldblib.o liolib.o lmathlib.o \
	loadlib.o loslib.o lstrlib.o ltablib.o lutf8lib.o linit.o
LUAR_O=	luar_lib.o luar_jit.o luar_bit.o $(FFI_O)
BASE_O= $(CORE_O) $(LIB_O) $(LUAR_O)

LUA_T=	luar
LUA_O=	lua.o

LUAC_T=	luarc
LUAC_O=	luac.o

ALL_O= $(BASE_O) $(LUA_O) $(LUAC_O)
ALL_T= $(LUA_A) $(LUA_T) $(LUAC_T)

# Targets start here.
default: all

all:	$(ALL_T)

o:	$(ALL_O)

a:	$(LUA_A)

$(LUA_A): $(BASE_O)
	$(AR) $(ARFLAGS) $@ $(BASE_O)
	$(RANLIB) $@

$(LUA_T): $(LUA_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(MYLDFLAGS) $(LUA_O) $(LUA_A) $(LIBS)

$(LUAC_T): $(LUAC_O) $(LUA_A)
	$(CC) -o $@ $(LDFLAGS) $(MYLDFLAGS) $(LUAC_O) $(LUA_A) $(LIBS)


%.o: %.c
	$(CC) $(STD) $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS) -c $<

test:
	./$(LUA_T) -v

clean:
	$(RM) $(ALL_T) $(ALL_O)

echo:
	@echo "CC= $(CC)"
	@echo "CFLAGS= $(CFLAGS) $(SYSCFLAGS) $(MYCFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"
	@echo "WITH_FFI= $(WITH_FFI)"

.PHONY: all default o a test clean echo
