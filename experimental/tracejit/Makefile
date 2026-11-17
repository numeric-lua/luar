# Build the LuaR trace-JIT DynASM toolchain proof-of-concept.
# x86-64 Linux SysV only (DynASM x64, no Windows ABI support here).
#
#   make            build ./demo
#   make run        build and run
#   make clean      remove generated/build artifacts
#
# The top-level library is built on demand if src/libluar.a is missing.

ROOT     := ../..
SRC      := $(ROOT)/src
DASMDIR  := $(ROOT)/dynasm
DYNASM   := $(DASMDIR)/dynasm.lua

LUA      ?= lua
CC       ?= cc
CFLAGS   ?= -O2 -g -Wall -Wextra
CPPFLAGS += -I$(SRC) -I$(DASMDIR)
LDFLAGS  ?=
LDLIBS   := $(SRC)/libluar.a -lm -ldl

.DELETE_ON_ERROR:

all: demo

# The generator is invoked exactly as required (just path-adjusted):
#   lua ../../dynasm/dynasm.lua -D X64 demo.dasc > demo_trace.c
demo_trace.c: demo.dasc $(DYNASM) $(DASMDIR)/dasm_ops.lua
	$(LUA) $(DYNASM) -D X64 demo.dasc > $@

demo_trace.o: demo_trace.c trace.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ demo_trace.c

demo.o: demo.c trace.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ demo.c

$(SRC)/libluar.a:
	$(MAKE) -C $(ROOT)

demo: demo.o demo_trace.o $(SRC)/libluar.a
	$(CC) $(LDFLAGS) -o $@ demo.o demo_trace.o $(LDLIBS)

run: demo
	./demo

clean:
	rm -f demo demo.o demo_trace.o demo_trace.c

.PHONY: all run clean
