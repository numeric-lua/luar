/*
** luar_emit.c -- wrapper that compiles the DynASM-generated emitter.
**
** The real code lives in luar_emit_x64.h, generated from
** luar_emit_x64.dasc by dynasm/dynasm.lua and checked in.  J2 currently
** targets x86-64 only; other hosts keep the interpreter (and J1 recorder).
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#include "luar_emit.h"

#if defined(__x86_64__) || defined(_M_X64)
#include "luar_emit_x64.h"

int luari_emit_available (void) {
  return 1;
}

#else

int luari_emit_available (void) {
  return 0;
}

LuaR_LoopFn luari_emit_build (const LuaR_LoopDesc *d, size_t *code_size) {
  (void)d;
  (void)code_size;
  return NULL;
}

int luari_emit_trace_ir (const LuaR_TraceIR *t, void **code, size_t *size) {
  (void)t; (void)code; (void)size;
  return 0;
}

#endif
