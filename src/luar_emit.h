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
  LUAR_OP_LOADF,
  LUAR_OP_LOADK,
  LUAR_OP_ADDI,
  LUAR_OP_ADD,
  LUAR_OP_SUB,
  LUAR_OP_MUL,
  LUAR_OP_DIV,
  LUAR_OP_ADDK,
  LUAR_OP_SUBK,
  LUAR_OP_MULK,
  LUAR_OP_DIVK,
  LUAR_OP_MMBIN,
  /* Branch support (int-only loops): comparisons and jumps. */
  LUAR_OP_LT, LUAR_OP_LE, LUAR_OP_GT, LUAR_OP_GE, LUAR_OP_EQ,
  LUAR_OP_JMP, LUAR_OP_NOP, LUAR_OP_GETTABLE, LUAR_OP_TABLE_ADD,
  LUAR_OP_SETTABLE
} LuaR_OpKind;

/* Register flavours known to the compiled loop. */
enum { LUAR_T_INT = 1, LUAR_T_FLOAT = 2, LUAR_T_TABLE = 3 };

typedef struct LuaR_Op {
  uint8_t kind;
  uint8_t a, b, c;
  uint8_t ta, tb, tc;	/* result and operand flavours (LUAR_T_*) */
  uint8_t k;		/* comparison jump condition */
  int32_t target;	/* branch target instruction index */
  int64_t imm;		/* LOADI/LOADK integer or constant value */
  double num;		/* LOADF value or constant value */
} LuaR_Op;

typedef struct LuaR_LoopDesc {
  const LuaR_Op *ops;
  int nops;
  int for_a;		/* FORLOOP A: count, step and index slots */
  int tag_int;		/* LUA_VNUMINT value for the guards */
  int tag_flt;		/* LUA_VNUMFLT value for the guards */
  int nguards;
  int npc;			/* instruction labels for branch loops */
  int has_branch;
  int is_while;			/* loop ends in a backward JMP, no FORLOOP */
  int has_table;		/* one raw array lookup at the body start */
  int table_reg;
  int table_array_off;
  int table_asize_off;
  int table_metatable_off;
  int tag_table;
  int fast_float;		/* acc += idx * constant fast path */
  int ff_acc;
  int ff_idx;
  double ff_const;
  int fast_fill;		/* table[idx] = idx fast path */
  int ff_table;
  uint8_t guard_reg[256];
  uint8_t guard_type[256];	/* LUAR_T_INT or LUAR_T_FLOAT */
} LuaR_LoopDesc;

/* x86-64 SysV: int f(void *base), base points at register R0. */
typedef int (*LuaR_LoopFn) (void *base);

/*
** Build executable code.  Returns NULL when the descriptor cannot be
** emitted (unsupported operation, mmap/mprotect failure, ...).  The caller
** owns the mapping and must munmap(*code_size) bytes when done.
*/
LuaR_LoopFn luari_emit_build (const LuaR_LoopDesc *d, size_t *code_size);

/* Nonzero when this build can emit and execute native loops on the host. */
int luari_emit_available (void);

#endif
