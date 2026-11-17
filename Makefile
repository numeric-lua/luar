# LuaR -- a Lua 5.5-forward runtime with LuaJIT-style extensions.
#
# Top-level build/install driver.  The engine itself is in src/ and is
# derived from the official Lua 5.5.1 sources (see vendor/lua-5.5.1).

PREFIX?= /usr/local
DESTDIR?=
WITH_FFI?= 0

MAKE?= make
RM= rm -f

VERSION= 0.1.0
LUAVER= 5.5

INSTALL_BIN= $(DESTDIR)$(PREFIX)/bin
INSTALL_LIB= $(DESTDIR)$(PREFIX)/lib
INSTALL_INC= $(DESTDIR)$(PREFIX)/include/luar
INSTALL_PC= $(DESTDIR)$(PREFIX)/lib/pkgconfig
INSTALL_MAN= $(DESTDIR)$(PREFIX)/share/man/man1
INSTALL_LMOD= $(DESTDIR)$(PREFIX)/share/luar/$(LUAVER)
INSTALL_CMOD= $(DESTDIR)$(PREFIX)/lib/luar/$(LUAVER)

MAKE_SRC= $(MAKE) -C src PREFIX="$(PREFIX)" WITH_FFI=$(WITH_FFI)

.PHONY: all build shared test test-semantics test-compat test-ffi bench \
	install uninstall clean distclean upstream-check sync-diff docs help

all: build

build:
	$(MAKE_SRC) all

# Convenience: build a shared library with the Lua 5.5 ABI exported.
shared:
	$(MAKE_SRC) libluar.so

test: build
	@echo "== smoke =="
	./src/luar -v
	./src/luar tests/smoke.lua
	@echo "== cli / abi =="
	./tests/cli.sh
	@echo "== suites =="
	./src/luar tests/run.lua semantics compat
	@if [ "$(WITH_FFI)" = "1" ]; then ./src/luar tests/run.lua ffi; fi

test-semantics: build
	./src/luar tests/run.lua semantics

test-compat: build
	./src/luar tests/run.lua compat

test-ffi: build
	./src/luar tests/run.lua ffi

test-all: test-semantics test-compat
	@echo "all test suites passed"

bench: build
	./src/luar tests/run.lua bench

install: build
	mkdir -p $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_INC) $(INSTALL_PC) \
		$(INSTALL_MAN) $(INSTALL_LMOD) $(INSTALL_CMOD)
	install -m 0755 src/luar src/luarc $(INSTALL_BIN)
	install -m 0644 src/libluar.a $(INSTALL_LIB)/libluar.a
	install -m 0644 src/lua.h src/luaconf.h src/lualib.h src/lauxlib.h \
		src/lua.hpp src/luar.h src/luar_version.h $(INSTALL_INC)
	install -m 0644 etc/luar.pc $(INSTALL_PC)/luar.pc
	install -m 0644 man/luar.1 $(INSTALL_MAN)/luar.1

uninstall:
	$(RM) $(INSTALL_BIN)/luar $(INSTALL_BIN)/luarc
	$(RM) $(INSTALL_LIB)/libluar.a
	$(RM) -r $(INSTALL_INC)
	$(RM) $(INSTALL_PC)/luar.pc
	$(RM) $(INSTALL_MAN)/luar.1

clean:
	$(MAKE_SRC) clean
	$(RM) -r build

# FAIL if src/ has drifted from the vendored upstream snapshot in ways
# that were not recorded in src/luar.patch.  See doc/upstream.md.
upstream-check:
	tools/upstream-check.sh

docs:
	@echo "Documentation lives in doc/ and README.md"

help:
	@sed -n '2,20p' Makefile
