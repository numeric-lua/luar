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
PREFIX_STAMP= src/.prefix
FFI_STAMP= src/.ffi

.PHONY: all build config-check shared test test-semantics test-compat test-ffi test-diff test-jit \
	test-upstream bench install uninstall clean distclean upstream-check sync-diff docs help

all: build

# PREFIX and WITH_FFI change compiler flags, which make cannot see, so a
# change to either must force a clean rebuild.
config-check:
	@mkdir -p $(dir $(PREFIX_STAMP))
	@if [ -f $(PREFIX_STAMP) ] && [ "$$(cat $(PREFIX_STAMP))" = "$(PREFIX)" ]; then :; \
	 else echo "$(PREFIX)" > $(PREFIX_STAMP); $(MAKE_SRC) clean; fi
	@if [ -f $(FFI_STAMP) ] && [ "$$(cat $(FFI_STAMP))" = "$(WITH_FFI)" ]; then :; \
	 else echo "$(WITH_FFI)" > $(FFI_STAMP); $(MAKE_SRC) clean; fi

build: config-check
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
	./src/luar tests/run.lua semantics compat jit
	@if [ "$(WITH_FFI)" = "1" ]; then ./src/luar tests/run.lua ffi; fi
	@echo "== differential vs stock lua =="
	./tests/diff/run.sh

test-semantics: build
	./src/luar tests/run.lua semantics

test-compat: build
	./src/luar tests/run.lua compat

test-ffi: build
	./src/luar tests/run.lua ffi

test-diff: build
	./tests/diff/run.sh

test-jit: build
	./src/luar tests/run.lua jit

# Official Lua test suite under src/luar (downloads into tests/tmp/).
test-upstream: build
	tests/upstream/run.sh

test-all: test-semantics test-compat
	@echo "all test suites passed"

bench: build
	./src/luar bench/bench.lua all

# Full cross-interpreter comparison (lua, luajit, luajit -joff, luar).
bench-compare: build
	./bench/compare.sh

install: build
	mkdir -p $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_INC) $(INSTALL_PC) \
		$(INSTALL_MAN) $(INSTALL_LMOD) $(INSTALL_CMOD)
	install -m 0755 src/luar src/luarc $(INSTALL_BIN)
	install -m 0644 src/libluar.a $(INSTALL_LIB)/libluar.a
	install -m 0644 src/lua.h src/luaconf.h src/lualib.h src/lauxlib.h \
		src/lua.hpp src/luar.h src/luar_version.h $(INSTALL_INC)
	sed 's|^prefix=.*|prefix=$(PREFIX)|' etc/luar.pc > $(INSTALL_PC)/luar.pc
	install -m 0644 man/luar.1 $(INSTALL_MAN)/luar.1

uninstall:
	$(RM) $(INSTALL_BIN)/luar $(INSTALL_BIN)/luarc
	$(RM) $(INSTALL_LIB)/libluar.a
	$(RM) -r $(INSTALL_INC)
	$(RM) $(INSTALL_PC)/luar.pc
	$(RM) $(INSTALL_MAN)/luar.1

clean:
	$(MAKE_SRC) clean
	$(RM) -r build $(PREFIX_STAMP) $(FFI_STAMP)

# FAIL if src/ has drifted from the vendored upstream snapshot in ways
# that were not recorded in tools/upstream-baseline.txt.  See doc/upstream.md.
upstream-check:
	tools/upstream-check.sh

docs:
	@echo "Documentation lives in doc/ and README.md"

help:
	@sed -n '2,20p' Makefile
