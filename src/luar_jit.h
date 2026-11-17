/*
** luar_jit.h -- internal LuaR JIT/trace-recording interface.
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
#define LUAR_JIT_MAXINS		192

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
  LuaR_Trace trace[LUAR_JIT_MAXTRACES];
} LuaR_JIT;


/* Lifecycle (luar_jit.c). */
LUAI_FUNC void luar_jit_init (lua_State *L);
LUAI_FUNC void luar_jit_shutdown (lua_State *L);

/* Called once per loop when its counter reaches LUAR_JIT_HOTCOUNT. */
LUAI_FUNC int luar_jit_hotloop (lua_State *L, Proto *p,
                                const Instruction *backedge);

/*
** Per-backedge fast path.  The epoch check makes jit.flush() reset the
** counters of every Proto lazily without walking the heap.
*/
static inline int luar_jit_hot (lua_State *L, Proto *p,
                                const Instruction *backedge) {
  LuaR_JIT *J = G(L)->luar_jit;
  uint32_t epoch = (J != NULL) ? J->epoch : 0;
  if (p->luar_epoch != epoch) {
    p->luar_epoch = epoch;
    p->luar_hot = 0;
  }
  if (p->luar_hot >= LUAR_JIT_HOTCOUNT)
    return 0;
  if (++p->luar_hot < LUAR_JIT_HOTCOUNT)
    return 0;
  return luar_jit_hotloop(L, p, backedge);
}

#endif
