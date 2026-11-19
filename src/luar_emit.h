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

#include "luar_trace.h"

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
  int pc;		/* Proto instruction index */
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
  int is_while;
  int repeat;
  int partial;			/* numeric prefix ending at CALL */
  int exit_pc;
  int entry_pc;			/* loop target instruction index */
  int fast_repeat;
  int fr_acc;
  int fr_idx;
  int fr_limit;
  int fr_imm;
  int64_t fr_immediate;
  int fr_step;
  int fr_kind;			/* LT/LE/GT/GE/EQ */
  int fr_k;
  int fr_idx_left;
  int fr_op;			/* 0 add, 1 sub, 2 mul, 3 square */
  int fr_float;			/* loop ends in a backward JMP, no FORLOOP */
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
  int ff_is_float;
  int64_t ff_iconst;
  int fast_meta;		/* inlined __index multiplier loop */
  int fm_table;
  int fm_acc;
  int fm_upval;
  int64_t fm_mult;
  int fast_closure;		/* inlined simple upvalue accumulator closure */
  int fc_func_slot;
  int fc_upval_slot;
  void *fc_closure;
  int fast_fill;		/* table[idx] = idx fast path */
  int ff_table;
  int fast_int;			/* acc += idx fast path */
  int fi_float;			/* float accumulator */
  int fi_acc;
  int fast_tsum;		/* acc += table[idx] fast path */
  int ft_table;
  int ft_idx;
  int ft_acc;
  int fast_sub;			/* acc = acc - idx fast path */
  int fsb_acc;
  int fsb_float;
  int fast_prod;		/* acc = acc * idx fast path */
  int fp_acc;
  int fp_float;
  int fp_div;
  int fast_sq;			/* acc += idx * idx fast path */
  int fs_acc;
  int fs_float;
  int fast_while;		/* while idx < limit do acc += idx end */
  int fw_acc;
  int fw_idx;
  int fw_limit;			/* register, or 0xFF for an immediate */
  int fw_square;
  int fw_prod;
  int fw_sub;
  int fw_div;
  int fw_linear;
  int fw_float;
  double fw_const;
  int fw_kind;			/* LUAR_OP_LT or LUAR_OP_LE */
  int fw_k;
  int64_t fw_step;
  int64_t fw_imm;
  uint8_t guard_reg[256];
  uint8_t guard_type[256];	/* LUAR_T_INT or LUAR_T_FLOAT */
} LuaR_LoopDesc;

/* x86-64 SysV: int f(void *base), base points at register R0. */
typedef int (*LuaR_LoopFn) (void *base, lua_State *L);

/*
** Build executable code.  Returns NULL when the descriptor cannot be
** emitted (unsupported operation, mmap/mprotect failure, ...).  The caller
** owns the mapping and must munmap(*code_size) bytes when done.
*/
LuaR_LoopFn luari_emit_build (const LuaR_LoopDesc *d, size_t *code_size);
int luari_emit_trace_ir (const LuaR_TraceIR *t, void **code, size_t *size);

/* Nonzero when this build can emit and execute native loops on the host. */
int luari_emit_available (void);

#endif
