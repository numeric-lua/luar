/*
** luar_emit.h -- native code emission for J2 numeric loops.
**
** The emitter turns a whitelisted loop body (a short list of register
** arithmetic operations) into an x86-64 function that runs the remaining
** iterations of a numeric for-loop directly on the Lua stack slots.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_emit_h
#define luar_emit_h

#include <stddef.h>
#include <stdint.h>

typedef enum {
  LUAR_OP_MOVE,
  LUAR_OP_LOADI,
  LUAR_OP_ADDI,
  LUAR_OP_ADD,
  LUAR_OP_SUB,
  LUAR_OP_MUL,
  LUAR_OP_MMBIN
} LuaR_OpKind;

typedef struct LuaR_Op {
  uint8_t kind;
  uint8_t a, b, c;
  int32_t imm;
} LuaR_Op;

typedef struct LuaR_LoopDesc {
  const LuaR_Op *ops;
  int nops;
  int for_a;		/* FORLOOP A: count, step and index slots */
  int tag_int;		/* LUA_VNUMINT value for the guards */
} LuaR_LoopDesc;

/* x86-64 SysV: int f(void *base), base points at register R0. */
typedef int (*LuaR_LoopFn) (void *base);

/*
** Build executable code.  Returns NULL when the descriptor cannot be
** emitted (unsupported operation, mmap/mprotect failure, ...).  The caller
** owns the mapping and must munmap(*code_size) bytes when done.
*/
LuaR_LoopFn luar_emit_build (const LuaR_LoopDesc *d, size_t *code_size);

/* Nonzero when this build can emit and execute native loops on the host. */
int luar_emit_available (void);

#endif
