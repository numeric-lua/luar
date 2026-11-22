/*
** luar_jit.h -- internal Luar JIT/trace-recording interface.
**
** J1 (this stage) records hot loop backedges and snapshots the bytecode of
** the loop into a trace buffer.  It does not generate machine code: the
** interpreter remains the only execution engine and jit.status() stays
** false.  See doc/jit-design.md sections 7 and 10.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_jit_h
#define luar_jit_h

#include <stddef.h>
#include <stdint.h>

#include "lua.h"

#include "lstate.h"
#include "lobject.h"


/* Backedge executions before a loop is considered hot (LuaJIT's default). */
#define LUAR_JIT_HOTCOUNT	56

/* Trace storage bounds.  J1 keeps bytecode snapshots, not Proto pointers,
** so a trace never keeps a collectable object alive or dangles. */
#define LUAR_JIT_MAXTRACES	64
#define LUAR_JIT_MAXINS		1024

#define LUAR_JIT_MAXCODE	64

typedef struct LuaR_Code {
  void *code;
  size_t size;
} LuaR_Code;

typedef struct LuaR_Trace {
  uint32_t startpc;			/* first instruction (1-based) */
  uint32_t endpc;			/* backedge instruction */
  uint32_t nins;
  Instruction code[LUAR_JIT_MAXINS];	/* snapshot of the loop */
} LuaR_Trace;

typedef struct LuaR_JIT {
  uint32_t epoch;		/* bumped by jit.flush(): stale counters */
  uint32_t hotloops;		/* loops that reached the threshold */
  uint32_t traces;		/* recorded bytecode snapshots */
  uint32_t aborts;		/* loops too large / not decodable */
  int enabled;			/* jit.off() clears this */
  uint32_t hotloop;		/* jit.opt.start("hotloop=N") */
  uint32_t hotexit;		/* guard exits before a variant is recorded */
  int enable_table;		/* experimental J3 table lookups (opt-in) */
  int enable_tint;		/* raw table fill/sum intrinsics (default on) */
  int enable_partial;		/* experimental numeric prefixes before CALL */
  int dump_trace;		/* jit.dump("t"): print bytecode snapshots */
  int dump_ir;			/* jit.dump("i"): print linear IR */
  uint32_t compiled;		/* native loops emitted */
  unsigned long long exits;	/* native guard failures (side-exit seeds) */
  uint32_t side_traces;		/* variants recorded from guard exits */
  int last_exit_pc;		/* exact mid-loop exit instruction, -1 if none */
  uint32_t sig_hits;		/* entry guard-signature dispatches */
  uint32_t ir_traces;		/* J4 IR traces recorded */
  uint32_t ir_ops;		/* J4 IR instructions recorded */
  uint32_t ir_aborts;		/* loops the J4 recorder rejected */
  uint32_t ir_side_traces;	/* IR segments recorded from guard exits */
  uint32_t ir_side_ops;
  uint32_t ir_side_aborts;
  uint32_t ir_max_depth;
  uint32_t ir_cycles;
  uint32_t ir_compiled;
  uint32_t ir_code_bytes;
  uint32_t ir_executed;
  uint32_t ir_entry_checks;
  uint32_t ir_entry_pc_mismatch;
  uint32_t ir_entry_base_mismatch;
  uint32_t ir_entry_failures;
  int32_t ir_last_entry_result;
  uint32_t ir_side_compiled;
  uint32_t ir_side_executed;
  uint32_t ir_call_exits;
  uint32_t ir_cont_compiled;
  uint32_t ir_cont_runs;
  uint32_t ir_install_rejects;
  unsigned long long ir_alloc_tables;	/* trace-side TNEW allocations */
  int ir_side_start;
  int ir_side_parent;	/* root loop start that produced the side exit */
  int ir_root_start;
  uint32_t ir_yields;
  int ir_side_depth;
  int enable_ir;
  int enable_alloc;		/* opt-in trace-side allocation (TNEW) */
  size_t mcode;			/* executable bytes */
  int ncode;
  int active_depth;		/* native traces currently executing */
  int flush_pending;		/* jit.flush() requested during a trace */
  LuaR_Code code[LUAR_JIT_MAXCODE];
  LuaR_Trace trace[LUAR_JIT_MAXTRACES];
} LuaR_JIT;


/* Lifecycle (luar_jit.c). */
LUAI_FUNC void luari_jit_init (lua_State *L);
LUAI_FUNC void luari_jit_shutdown (lua_State *L);

/* Called once per loop when its counter reaches LUAR_JIT_HOTCOUNT. */
LUAI_FUNC int luari_jit_hotloop (lua_State *L, Proto *p,
                                const Instruction *backedge, void *base);

/* Try to run the compiled loop for 'p' on the given stack base.  Returns 1
** if the loop was completed natively, 0 to keep interpreting. */
LUAI_FUNC int luari_jit_continue (lua_State *L, Proto *p, int pc, void *base);
int luari_jit_try_run (lua_State *L, Proto *p, int pc,
                                void *base, int expected_base);


/*
** Per-backedge fast path.  Once a Proto is hot the branch below is the only
** cost (one load and compare); the recorder is called once, at the
** threshold.  jit.flush() clears the trace list and stats but does not walk
** the heap to reset already-hot prototypes (J1 limitation; J2 keeps a
** prototype list).
*/
static inline int luari_jit_hot (lua_State *L, Proto *p,
                                const Instruction *backedge, void *base) {
  LuaR_JIT *J = G(L)->luar_jit;
  if (p->luar_hot > p->luar_hot_limit)
    return 0;
  if (p->luar_hot == p->luar_hot_limit) {
    unsigned pc;
    unsigned long long bit;
    /* LUAR: once a loop is hot, keep offering *different* backedges to the
    ** compiler.  A rejected loop must not disable its siblings; the seen
    ** bloom below remembers each attempted backedge individually.  When the
    ** opt-in IR tier already owns this Proto, leave the root/side trace
    ** model alone: re-entering the hot hook would replace its root stats. */
    if (J != NULL && J->enable_ir && p->luar_ir_code != NULL &&
        p->luar_ir_epoch == J->epoch)
      return 0;
    pc = (unsigned)(backedge - p->code);
    bit = 1ULL << (pc & 63);
    if ((p->luar_seen & bit) != 0)
      return 0;
    p->luar_seen |= bit;
    return luari_jit_hotloop(L, p, backedge, base);
  }
  if (++p->luar_hot < p->luar_hot_limit)
    return 0;
  p->luar_seen |= 1ULL << (((unsigned)(backedge - p->code)) & 63);
  return luari_jit_hotloop(L, p, backedge, base);
}

#if defined(LUAR_JIT_DISABLE)
#define LUAR_JIT_HOT(L, p, pc, base)	((void)(base), (void)0)
#define LUAR_JIT_RUN(L, p, pc, base, exp)	0
#else
#define LUAR_JIT_HOT(L, p, pc, base)	luari_jit_hot((L), (p), (pc), (base))
#define LUAR_JIT_RUN(L, p, pc, base, exp) \
  ((((p)->luar_code[0] != NULL) || ((p)->luar_code[1] != NULL) || \
     ((p)->luar_tint[0] != 0) || ((p)->luar_tint[1] != 0) || \
     ((p)->luar_ir_code != NULL)) ? \
   luari_jit_try_run((L), (p), (pc), (base), (exp)) : 0)
#endif

#endif
