# Luar -- a Lua 5.5-forward runtime with LuaJIT-style extensions.
#
# Top-level build/install driver.  The engine itself is in src/ and is
# derived from the official Lua 5.5.1 sources (see vendor/lua-5.5.1).

PREFIX?= /usr/local
DESTDIR?=
WITH_FFI?= 0
JIT?= 1
WITH_JIT?= $(JIT)

MAKE?= make
RM= rm -f

VERSION= 0.206.0
LUAVER= 5.5
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
DISTNAME= luar-$(VERSION)

INSTALL_BIN= $(DESTDIR)$(PREFIX)/bin
INSTALL_LIB= $(DESTDIR)$(PREFIX)/lib
INSTALL_INC= $(DESTDIR)$(PREFIX)/include/luar
INSTALL_PC= $(DESTDIR)$(PREFIX)/lib/pkgconfig
INSTALL_MAN= $(DESTDIR)$(PREFIX)/share/man/man1
INSTALL_LMOD= $(DESTDIR)$(PREFIX)/share/luar/$(LUAVER)
INSTALL_CMOD= $(DESTDIR)$(PREFIX)/lib/luar/$(LUAVER)

MAKE_SRC= $(MAKE) -C src PREFIX="$(PREFIX)" WITH_FFI=$(WITH_FFI) WITH_JIT=$(WITH_JIT) VERSION=$(VERSION)
PREFIX_STAMP= src/.prefix
FFI_STAMP= src/.ffi
JIT_STAMP= src/.jit

.PHONY: all build config-check shared test test-shared test-semantics test-compat \
	test-ffi test-diff test-jit test-asan test-fuzz test-consumer test-upstream \
	bench install uninstall clean distclean upstream-check sync-diff docs help \
	dist test-dist

all: build

# PREFIX and WITH_FFI change compiler flags, which make cannot see, so a
# change to either must force a clean rebuild.
config-check:
	@mkdir -p $(dir $(PREFIX_STAMP))
	@if [ -f $(PREFIX_STAMP) ] && [ "$$(cat $(PREFIX_STAMP))" = "$(PREFIX)" ]; then :; \
	 else echo "$(PREFIX)" > $(PREFIX_STAMP); $(MAKE_SRC) clean; fi
	@if [ -f $(FFI_STAMP) ] && [ "$$(cat $(FFI_STAMP))" = "$(WITH_FFI)" ]; then :; \
	 else echo "$(WITH_FFI)" > $(FFI_STAMP); $(MAKE_SRC) clean; fi
	@if [ -f $(JIT_STAMP) ] && [ "$$(cat $(JIT_STAMP))" = "$(WITH_JIT)" ]; then :; \
	 else echo "$(WITH_JIT)" > $(JIT_STAMP); $(MAKE_SRC) clean; fi

build: config-check
	$(MAKE_SRC) all

# Shared library: libluar.so.<version>, SONAME libluar.so.<major>, and the
# unversioned libluar.so symlink.  Uses separate .pic.o objects, so the
# static build is unaffected.
shared: config-check
	$(MAKE_SRC) shared

test: build
	@echo "== smoke =="
	./src/luar -v
	./src/luar tests/smoke.lua
	@echo "== cli / abi =="
	./tests/cli.sh
	@echo "== suites =="
	./src/luar tests/run.lua semantics compat jit consumer
	@if [ "$(WITH_FFI)" = "1" ]; then ./src/luar tests/run.lua ffi; fi
	@echo "== differential vs stock lua =="
	./tests/diff/run.sh

# Link an embedding host and the CLI against libluar.so, then load a stock
# Lua 5.5 C module.  See tests/shared.sh.
test-shared: build shared
	./tests/shared.sh

test-semantics: build
	./src/luar tests/run.lua semantics

test-compat: build
	./src/luar tests/run.lua compat

test-ffi: WITH_FFI=1
test-ffi: build
	./src/luar tests/run.lua ffi

test-diff: build
	./tests/diff/run.sh

test-jit: build
	./src/luar tests/run.lua jit

test-consumer: build
	./src/luar tests/run.lua consumer

# Sanitizer pass: tests/asan.sh does its own clean build, so this target
# deliberately has no build prerequisite.  The script defaults to WITH_FFI=1;
# an explicit WITH_FFI=0/1 on the make command line (or in the environment)
# is forwarded so "make WITH_FFI=0 test-asan" tests the stub build too.
ASAN_FFI= $(if $(filter command line environment,$(origin WITH_FFI)),$(WITH_FFI),1)

test-asan:
	WITH_FFI=$(ASAN_FFI) tests/asan.sh

# Deterministic ffi.cdef fuzzer.  The FFI is opt-in, so this target forces an
# FFI build even when the caller did not pass WITH_FFI=1.  Override FUZZ_ARGS
# to change the number of cases or the seed; see tools/fuzz-cdef.lua.
FUZZ_ARGS?= --cases 5000 --max-seconds 4

test-fuzz: WITH_FFI=1
test-fuzz: build
	@set -eu; \
	if command -v timeout >/dev/null 2>&1; then \
	  timeout 120 ./src/luar tools/fuzz-cdef.lua $(FUZZ_ARGS); \
	else \
	  ./src/luar tools/fuzz-cdef.lua $(FUZZ_ARGS); \
	fi

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

bench-jit: build
	./tools/bench-jit-compare.sh

install: build shared
	mkdir -p $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_INC) $(INSTALL_PC) \
		$(INSTALL_MAN) $(INSTALL_LMOD) $(INSTALL_CMOD)
	$(RM) $(INSTALL_LIB)/libluar.so.* $(INSTALL_LIB)/libluar.*.dylib
	install -m 0755 src/luar src/luarc $(INSTALL_BIN)
	install -m 0644 src/libluar.a $(INSTALL_LIB)/libluar.a
	install -m 0755 src/$(LUA_SO) $(INSTALL_LIB)/$(LUA_SO)
	ln -sfn $(LUA_SO) $(INSTALL_LIB)/$(LUA_SONAME)
	ln -sfn $(LUA_SONAME) $(INSTALL_LIB)/$(LUA_SOLINK)
	install -m 0644 src/lua.h src/luaconf.h src/lualib.h src/lauxlib.h \
		src/lua.hpp src/luar.h src/luar_version.h $(INSTALL_INC)
	sed -e 's|^prefix=.*|prefix=$(PREFIX)|' \
	    -e '$(if $(filter Darwin,$(UNAME_S)),s|^Libs.private: -ldl$$|Libs.private:|,)' \
	    etc/luar.pc > $(INSTALL_PC)/luar.pc
	install -m 0644 man/luar.1 $(INSTALL_MAN)/luar.1

dist:
	@test -z "$$(git status --porcelain)" || { 	  echo "dist: working tree is dirty; commit or stash changes first" >&2; 	  exit 1; }
	git archive --format=tar --prefix=$(DISTNAME)/ HEAD | gzip -n > $(DISTNAME).tar.gz

test-dist: dist
	rm -rf tests/tmp/dist && mkdir -p tests/tmp/dist
	tar -xzf $(DISTNAME).tar.gz -C tests/tmp/dist
	$(MAKE) -C tests/tmp/dist/$(DISTNAME) build
	./tests/tmp/dist/$(DISTNAME)/src/luar -v | grep -q 'Luar $(VERSION) (Lua 5.5'

uninstall:
	$(RM) $(INSTALL_BIN)/luar $(INSTALL_BIN)/luarc
	$(RM) $(INSTALL_LIB)/libluar.a
	$(RM) $(INSTALL_LIB)/$(LUA_SO) $(INSTALL_LIB)/$(LUA_SONAME) \
		$(INSTALL_LIB)/$(LUA_SOLINK)
	$(RM) -r $(INSTALL_INC)
	$(RM) $(INSTALL_PC)/luar.pc
	$(RM) $(INSTALL_MAN)/luar.1

clean:
	$(MAKE_SRC) clean
	$(RM) -r build $(PREFIX_STAMP) $(FFI_STAMP) $(JIT_STAMP)

distclean: clean
	$(RM) $(DISTNAME).tar.gz
	$(RM) -r tests/tmp

# FAIL if src/ has drifted from the vendored upstream snapshot in ways
# that were not recorded in tools/upstream-baseline.txt.  See doc/upstream.md.
upstream-check:
	tools/upstream-check.sh

docs:
	@echo "Documentation lives in doc/ and README.md"

help:
	@sed -n '2,20p' Makefile
# Verify that the checked-in DynASM output matches its .dasc source.  Needs a
# host lua 5.5 (or luar) on PATH; not part of the default build.
dynasm-check:
	lua dynasm/dynasm.lua -D X64 src/luar_emit_x64.dasc | \
	  diff -u src/luar_emit_x64.h -
