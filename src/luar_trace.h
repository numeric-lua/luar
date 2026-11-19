/*
** luar_trace.h -- LuaR J4 tracing-IR and exit-state model.
**
** This is the first layer of a tracing JIT, deliberately independent from
** the existing one-pass loop emitter.  Traces are recorded as a small,
** explicitly verified IR; exit snapshots carry the tags and payloads of
** live stack slots so a side trace can be linked from an interior PC.
*/
#ifndef luar_trace_h
#define luar_trace_h

#include <stddef.h>
#include <stdint.h>

#include "lobject.h"

#define LUAR_IR_MAX_INS		64
#define LUAR_IR_MAX_EXITS	4
#define LUAR_IR_MAX_SLOTS	64
#define LUAR_IR_VERSION		1
#define LUAR_IR_MAX_DEPTH	4

typedef enum {
  LUAR_IR_KINT = 1,	/* value = signed integer constant */
  LUAR_IR_KFLOAT,	/* value = IEEE double constant */
  LUAR_IR_SLOAD,	/* a = destination slot, b = source slot */
  LUAR_IR_SSTORE,	/* a = slot, b = source slot */
  LUAR_IR_LOADI,	/* a = destination slot, ival = constant */
  LUAR_IR_ADDI,		/* a = destination, b = source, ival = immediate */
  LUAR_IR_ADDK,
  LUAR_IR_SUBK,
  LUAR_IR_MULK,
  LUAR_IR_DIVK,
  LUAR_IR_MODK,
  LUAR_IR_IDIVK,
  LUAR_IR_GUARD,	/* a = slot, b = LUAR_IR_TAG_* expected */
  LUAR_IR_ADD,		/* a = dest, b/c = operands */
  LUAR_IR_SUB,
  LUAR_IR_MUL,
  LUAR_IR_DIV,
  LUAR_IR_TGET,		/* a = dest, b = table, c = key */
  LUAR_IR_TSET,		/* a = table, b = key, c = value */
  LUAR_IR_CGUARD,	/* a = slot, ival = expected closure pointer */
  LUAR_IR_FORLOOP,	/* a = control base slot */
  LUAR_IR_EXIT,		/* a = exit index */
  LUAR_IR_RET
} LuaR_IRKind;

enum {
  LUAR_IR_TAG_INT = 1,
  LUAR_IR_TAG_FLOAT = 2,
  LUAR_IR_TAG_TABLE = 3,
  LUAR_IR_TAG_OTHER = 4
};

typedef struct LuaR_IRIns {
  uint8_t kind;
  uint8_t a, b, c;
  int32_t pc;		/* originating Proto instruction, -1 if synthetic */
  int64_t ival;
  double nval;
} LuaR_IRIns;

typedef struct LuaR_IRExitState {
  int32_t pc;			/* interpreter resume instruction */
  uint8_t nslots;
  uint8_t tags[LUAR_IR_MAX_SLOTS];
  uint64_t bits[LUAR_IR_MAX_SLOTS];
} LuaR_IRExitState;

typedef struct LuaR_TraceIR {
  Proto *proto;		/* informational; not owned */
  int32_t startpc;
  int32_t maxslots;
  int depth;		/* side-trace depth, 0 for a root trace */
  int side;		/* 1 = interior side segment */
  int nins;
  LuaR_IRIns ins[LUAR_IR_MAX_INS];
  int nexits;
  LuaR_IRExitState exits[LUAR_IR_MAX_EXITS];
} LuaR_TraceIR;

void luar_trace_ir_init (LuaR_TraceIR *t, Proto *proto, int32_t startpc,
                         int32_t maxslots, int depth);
int luar_trace_ir_emit (LuaR_TraceIR *t, const LuaR_IRIns *ins);
int luar_trace_ir_add_exit (LuaR_TraceIR *t, const LuaR_IRExitState *ex);
int luar_trace_ir_verify (const LuaR_TraceIR *t, char *err, size_t errlen);

/* Snapshot/restore live stack slots.  A snapshot is short-lived and must not
** be used across a GC safepoint; restoring into stack slots needs no barrier.
*/
int luar_trace_ir_snapshot (LuaR_IRExitState *ex, int32_t pc, TValue *base,
                            int nslots);
int luar_trace_ir_restore (const LuaR_IRExitState *ex, TValue *base);

int luar_trace_ir_analyze (LuaR_TraceIR *t, char *err, size_t errlen);
int64_t luar_trace_tget_int (TValue *tv, lua_Integer key, int64_t *out);
int64_t luar_trace_tget_generic (TValue *tv, lua_Integer key, int64_t *out);
int64_t luar_trace_tset_int (TValue *tv, lua_Integer key, TValue *val);
int luar_trace_ir_guards_match (const LuaR_TraceIR *t, TValue *base);
int luar_trace_ir_link (LuaR_TraceIR *t, int parent_depth, int root_start);
int luar_trace_record_loop (Proto *p, int start, int backedge, TValue *base,
                            LuaR_TraceIR *t);
int luar_trace_ir_selftest (void);

#endif
